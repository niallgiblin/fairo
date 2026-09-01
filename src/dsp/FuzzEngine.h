#pragma once

/**
 * @file
 * @brief The complete Fairo fuzz signal chain (PLAN.md Phase 1 + 2).
 *
 *   guitar ─➤ [Input RC: Hi/Lo series R + 470nF] ─➤ [Hi/Lo level shift]
 *          ─➤ [Fuzz gain] ─➤ [Clip 1: fixed 2x 1N914 (Newton)]
 *          ─➤ [Interstage RC: 470nF coupling]
 *          ─➤ [Stage-2 gain] ─➤ [Clip 2: Si / Ge / Bypass branch]
 *          ─➤ [Coupled Tone/High stack]
 *          ─➤ [Make-up gain + soft limit] ─➤ [Volume] ─➤ out
 *
 * Everything between the nonlinear clippers is solved as coupled linear RC
 * networks (@ref RcNetwork) — never two independent filters (PLAN.md pitfall
 * #1). All nonlinearities sit in the two clipping stages, both Shockley-based
 * Newton-Raphson solvers (@ref DiodeClipper) — level-dependent, per pitfall #2.
 *
 * The engine runs the nonlinear chain at 2x oversampling (PLAN.md Phase 4)
 * through JUCE's dsp::Oversampling polyphase filters.
 */

#include "DiodeClipper.h"
#include "RcNetwork.h"
#include "ToneStack.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace fairo
{

class FuzzEngine
{
public:
    enum class ClipMode { Silicon, Germanium, Bypass };

    FuzzEngine() = default;

    /** @brief Prepare at a sample rate (call once in prepareToPlay). */
    void prepare(double sampleRate, int maxBlockSize);

    /** @brief Clear all state (filters, clippers, smoothers). */
    void reset();

    /** @brief Process @a numSamples; src/dst may not alias. */
    void processBlock(const float* src, float* dst, int numSamples);

    // ── Parameters ─────────────────────────────────────────────────────────
    void setFuzz(float v) noexcept;    // 0..1  (Fuzz pot)
    void setVolume(float v) noexcept;  // 0..1  (Volume pot)
    void setTone(float v) noexcept;    // 0..1  (Tone pot)
    void setHigh(float v) noexcept;    // 0..1  (High pot)
    void setHiLo(bool hi) noexcept;    // Hi/Lo input switch
    void setClipMode(ClipMode mode) noexcept;

    ClipMode getClipMode() const noexcept { return clipMode; }
    bool getHiLo() const noexcept { return hiLo; }

private:
    /** One sample at the oversampled rate (no allocation). */
    float processSampleInternal(float x) noexcept;

    /** Rebuild the stage-2 branch (clipper config or bypass knee). */
    void applyClipMode() noexcept;

    RcNetwork inputNetwork;
    RcNetwork interstageNetwork;
    ToneStack toneStack;
    DiodeClipper clip1;
    DiodeClipper clip2;
    // One 2x polyphase allpass stage (JUCE: `factor` = number of 2x stages).
    juce::dsp::Oversampling<float> oversampling{
        1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fuzzGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> volumeSmoothed;

    // Post-clip presence high-shelf (RBJ 2nd-order) — brightens the top end.
    static void computePresenceShelfCoeffs(double sampleRate,
                                           double& b0, double& b1, double& b2,
                                           double& a1, double& a2) noexcept;
    void preparePresenceShelf(double sampleRate);
    inline float processPresence(float x) noexcept
    {
        const double y = shelfB0 * x + shelfB1 * shelfX1 + shelfB2 * shelfX2
                       - shelfA1 * shelfY1 - shelfA2 * shelfY2;
        shelfX2 = shelfX1; shelfX1 = x;
        shelfY2 = shelfY1; shelfY1 = static_cast<float>(y);
        return static_cast<float>(y);
    }
    double shelfB0 = 1.0, shelfB1 = 0.0, shelfB2 = 0.0, shelfA1 = 0.0, shelfA2 = 0.0;
    float shelfX1 = 0.0f, shelfX2 = 0.0f, shelfY1 = 0.0f, shelfY2 = 0.0f;

    juce::AudioBuffer<float> osBuffer;  // internal oversampled scratch (no audio-thread alloc)

    ClipMode clipMode = ClipMode::Silicon;
    bool hiLo = false;                  // false = Lo
    bool prepared = false;
};

} // namespace fairo
