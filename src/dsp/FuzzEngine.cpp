#include "FuzzEngine.h"

#include "CircuitValues.h"

#include <algorithm>
#include <cmath>

namespace fairo
{

namespace
{

constexpr float kFuzzMappedMin = static_cast<float>(circuit::kGainStage1Min);
constexpr float kFuzzMappedMax = static_cast<float>(circuit::kGainStage1Max);

/** Map the Fuzz pot (0..1) to the stage-1 gain (exponential-ish taper). */
float fuzzToGain(float v) noexcept
{
    const float t = std::clamp(v, 0.0f, 1.0f);
    const float ratio = kFuzzMappedMax / kFuzzMappedMin;
    return kFuzzMappedMin * std::pow(ratio, t);
}

/** Bypass branch: transistor-only saturation (thesis transfer shape). */
inline float bypassSaturate(float x, float vSat) noexcept
{
    const float a = std::abs(x);
    if (a < 1.0e-9f || a >= 1.0e6f * vSat)
        return x;  // near-linear / defencive on extreme magnitudes
    return vSat * (x / (vSat + a));
}

} // namespace

void FuzzEngine::prepare(double sampleRate, int maxBlockSize)
{
    const int maxOs = std::max(2, maxBlockSize * circuit::kOversamplingFactor);

    // Linear network topology (values in CircuitValues.h; see comments there).
    {
        using B = RcNetwork::Branch;
        using T = RcNetwork::Type;

        // Input stage (real): Hi/Lo series R (39k/390k) -> C1 10uF -> Q4
        // base-load (fitted so the two positions differ by ~15 dB, thesis 3.2.1).
        inputNetwork.setTopology(circuit::kInputResistanceLo, {
            B{ T::Capacitor, 0, 1, circuit::kInputDecouplingCap },
            B{ T::Resistor, 1, -1, circuit::kInputStageLoadR },
        }, 1);

        // Interstage: clip-1 node (R21 source) -> 47nF coupling -> Q2 bias 470k.
        interstageNetwork.setTopology(circuit::kClip1SeriesR, {
            B{ T::Capacitor, 0, 1, circuit::kInterstageCouplingCap },
            B{ T::Resistor, 1, -1, circuit::kInterstageBiasLeak },
        }, 1);

        // Tone/High stack (coupled network; see ToneStack).
        toneStack.setTonePot(0.5f);
        toneStack.setHighPot(0.5f);
    }

    inputNetwork.prepare(sampleRate * circuit::kOversamplingFactor);
    interstageNetwork.prepare(sampleRate * circuit::kOversamplingFactor);
    toneStack.prepare(sampleRate * circuit::kOversamplingFactor);

    clip1.setConfig(DiodeClipConfig::silicon1N914(circuit::kClip1SeriesR));
    applyClipMode();

    // JUCE Oversampling::initProcessing takes the max *input* (pre-OS) samples
    // per block and sizes its internal filters for 2x that.
    oversampling.initProcessing(static_cast<size_t>(std::max(2, maxBlockSize)));
    osBuffer.setSize(1, maxOs);

    fuzzGainSmoothed.reset(sampleRate * circuit::kOversamplingFactor, 0.01);
    volumeSmoothed.reset(sampleRate * circuit::kOversamplingFactor, 0.01);
    fuzzGainSmoothed.setCurrentAndTargetValue(fuzzToGain(0.5f));
    volumeSmoothed.setCurrentAndTargetValue(0.6f);
    preparePresenceShelf(sampleRate * circuit::kOversamplingFactor);

    prepared = true;
    reset();
}

void FuzzEngine::preparePresenceShelf(double sampleRate)
{
    computePresenceShelfCoeffs(sampleRate, shelfB0, shelfB1, shelfB2, shelfA1, shelfA2);
}

void FuzzEngine::computePresenceShelfCoeffs(double sampleRate,
                                            double& b0, double& b1, double& b2,
                                            double& a1, double& a2) noexcept
{
    // RBJ audio-EQ-cookbook 2nd-order high-shelf. Boosts above kPresenceShelfFc
    // by kPresenceShelfGainDb to restore top-end that the distortion's harmonic
    // content under-produces vs. the reference capture.
    const double A  = std::pow(10.0, circuit::kPresenceShelfGainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * circuit::kPresenceShelfFc / sampleRate;
    const double cs = std::cos(w0), sn = std::sin(w0);
    const double alpha = (sn / 2.0) * std::sqrt(2.0);            // shelf slope S = 1
    const double sA  = std::sqrt(A);
    const double bb0 =  A * ((A + 1.0) + (A - 1.0) * cs + 2.0 * sA * alpha);
    const double bb1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
    const double bb2 =  A * ((A + 1.0) + (A - 1.0) * cs - 2.0 * sA * alpha);
    const double a0  =  (A + 1.0) - (A - 1.0) * cs + 2.0 * sA * alpha;
    const double aa1 =  2.0 * ((A - 1.0) - (A + 1.0) * cs);
    const double aa2 =  (A + 1.0) - (A - 1.0) * cs - 2.0 * sA * alpha;
    b0 = bb0 / a0; b1 = bb1 / a0; b2 = bb2 / a0;
    a1 = aa1 / a0; a2 = aa2 / a0;
}

void FuzzEngine::reset()
{
    inputNetwork.reset();
    interstageNetwork.reset();
    toneStack.reset();
    clip1.reset();
    clip2.reset();
    fuzzGainSmoothed.reset(fuzzToGain(0.5f));
    volumeSmoothed.reset(0.6f);
    shelfX1 = shelfX2 = shelfY1 = shelfY2 = 0.0f;
    oversampling.reset();  // clear the polyphase filter state too
}

void FuzzEngine::processBlock(const float* src, float* dst, int numSamples)
{
    if (!prepared || numSamples <= 0 || src == nullptr || dst == nullptr)
        return;

    // ── Oversample (JUCE 8 API: processSamplesUp returns the upsample block) ─
    osBuffer.copyFrom(0, 0, src, numSamples);
    juce::dsp::AudioBlock<float> inBlock(osBuffer);
    inBlock = inBlock.getSubBlock(0, static_cast<size_t>(numSamples));

    juce::dsp::AudioBlock<float> upBlock = oversampling.processSamplesUp(inBlock);
    const int osSamples = static_cast<int>(upBlock.getNumSamples());

    float* up = upBlock.getChannelPointer(0);
    for (int i = 0; i < osSamples; ++i)
        up[i] = processSampleInternal(up[i]);

    // ── Downsample (writes numSamples floats back into osBuffer) ────────────
    juce::dsp::AudioBlock<float> outBlock(osBuffer);
    outBlock = outBlock.getSubBlock(0, static_cast<size_t>(numSamples));
    oversampling.processSamplesDown(outBlock);

    std::copy(osBuffer.getReadPointer(0),
              osBuffer.getReadPointer(0) + numSamples, dst);
}

float FuzzEngine::processSampleInternal(float x) noexcept
{
    // DC-block at the very front (input cap of the pedal).
    x = inputNetwork.processSample(x);

    // Fuzz drive into stage 1 (the Hi/Lo ~15 dB shift comes from the input
    // divider itself — R2/R27 vs the Q4 base-load).
    x *= fuzzGainSmoothed.getNextValue();

    // Clip stage 1: fixed 2x 1N914, symmetrical Newton-Raphson.
    x = clip1.processSample(x);

    // Interstage coupling.
    x = interstageNetwork.processSample(x);

    // Stage-2 gain (common-emitter); branch switch below.
    x *= static_cast<float>(circuit::kGainStage2);

    if (clipMode == ClipMode::Bypass)
        x = bypassSaturate(x, static_cast<float>(circuit::kBypassSaturationVoltage));
    else
        x = clip2.processSample(x);

    // Coupled Tone/High stack.
    x = toneStack.processSample(x);

    // Make-up gain + gentle output-stage saturation, then Volume.
    x *= static_cast<float>(circuit::kMakeupGain);
    x = static_cast<float>(circuit::kOutputSoftLimit)
        * std::tanh(x / static_cast<float>(circuit::kOutputSoftLimit));
    x = processPresence(x);   // post-clip top-end presence lift
    x *= volumeSmoothed.getNextValue();

    return x;
}

void FuzzEngine::setFuzz(float v) noexcept
{
    fuzzGainSmoothed.setTargetValue(fuzzToGain(std::clamp(v, 0.0f, 1.0f)));
}

void FuzzEngine::setVolume(float v) noexcept
{
    const float t = std::clamp(v, 0.0f, 1.0f);
    volumeSmoothed.setTargetValue(t * static_cast<float>(circuit::kVolumeMax));
}

void FuzzEngine::setTone(float v) noexcept
{
    toneStack.setTonePot(std::clamp(v, 0.0f, 1.0f));
}

void FuzzEngine::setHigh(float v) noexcept
{
    toneStack.setHighPot(std::clamp(v, 0.0f, 1.0f));
}

void FuzzEngine::setHiLo(bool hi) noexcept
{
    if (hiLo == hi)
        return;
    hiLo = hi;
    inputNetwork.setInputResistance(hi ? circuit::kInputResistanceHi
                                       : circuit::kInputResistanceLo);
}

void FuzzEngine::setClipMode(ClipMode mode) noexcept
{
    if (clipMode == mode)
        return;
    clipMode = mode;
    applyClipMode();
}

void FuzzEngine::applyClipMode() noexcept
{
    switch (clipMode)
    {
        case ClipMode::Silicon:
            clip2.setConfig(DiodeClipConfig::silicon1N4001(circuit::kClip2SeriesR));
            break;
        case ClipMode::Germanium:
            clip2.setConfig(DiodeClipConfig::germanium1N34A(circuit::kClip2SeriesR));
            break;
        case ClipMode::Bypass:
            clip2.setConfig(DiodeClipConfig::bypass());
            break;
    }
}

} // namespace fairo
