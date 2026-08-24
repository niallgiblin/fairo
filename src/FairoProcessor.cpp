#include "FairoProcessor.h"
#include "FairoEditor.h"

#include <algorithm>
#include <cmath>

FairoProcessor::FairoProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

FairoProcessor::~FairoProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout FairoProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "fuzz", 1 },
        "Fuzz",
        juce::NormalisableRange<float>{ 0.0f, 1.0f, 0.001f },
        0.5f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "volume", 1 },
        "Volume",
        juce::NormalisableRange<float>{ 0.0f, 1.0f, 0.001f },
        0.6f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tone", 1 },
        "Tone",
        juce::NormalisableRange<float>{ 0.0f, 1.0f, 0.001f },
        0.5f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "high", 1 },
        "High",
        juce::NormalisableRange<float>{ 0.0f, 1.0f, 0.001f },
        0.5f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "hiLo", 1 },
        "Hi/Lo",
        juce::StringArray{ "Lo", "Hi" },
        0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "clipMode", 1 },
        "Clip mode",
        juce::StringArray{ kClipModeSilicon, kClipModeGermanium, kClipModeBypass },
        0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "engine", 1 },
        "Engine",
        juce::StringArray{ "DSP", "Neural" },
        0));  // Neural falls back to DSP when models are unavailable

    return layout;
}

bool FairoProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    // Mono pedal: mono or stereo in; mono or stereo out (all channnels share
    // the mono instance — this matches how a real mono pedal is inserted).
    const bool inOk = (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
    const bool outOk = (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    return inOk && outOk;
}

void FairoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
    engine.prepare(sr, samplesPerBlock);

#if defined(FA_ENABLE_ONNX)
    // Phase 3: load the three branch models once (embedded via BinaryData).
    neuralModelsLoaded =
        neuralSi.loadModel(fairo::FuzzNeuralInference::Branch::Silicon,
                           BinaryData::fuzz_silicon_onnx, BinaryData::fuzz_silicon_onnxSize)
        && neuralGe.loadModel(fairo::FuzzNeuralInference::Branch::Germanium,
                              BinaryData::fuzz_germanium_onnx, BinaryData::fuzz_germanium_onnxSize)
        && neuralBy.loadModel(fairo::FuzzNeuralInference::Branch::Bypass,
                              BinaryData::fuzz_bypass_onnx, BinaryData::fuzz_bypass_onnxSize);
#endif

    prepared = true;
    pushParametersToEngine();
}

void FairoProcessor::releaseResources()
{
    prepared = false;
}

void FairoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || ! prepared)
        return;

    // Nil-safe access to the mono source: average stereo input channels,
    // else channel 0.
    const int numChannels = buffer.getNumChannels();
    const float* srcL = buffer.getReadPointer(0);
    const float* srcR = (numChannels > 1) ? buffer.getReadPointer(1) : nullptr;

    std::vector<float> mono(static_cast<size_t>(numSamples));
    if (srcR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
            mono[static_cast<size_t>(i)] = 0.5f * (srcL[i] + srcR[i]);
    }
    else
    {
        std::copy(srcL, srcL + numSamples, mono.data());
    }

    // Scrub non-finite samples (defensive). The engine is guaranteed
    // NaN-free for finite inputs, but a corrupt host buffer must never
    // persist through the plugin.
    for (auto& s : mono)
        if (! std::isfinite(s))
            s = 0.0f;

    pushParametersToEngine();

    float* monoOut = mono.data();
    engine.processBlock(monoOut, monoOut, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.copyFrom(ch, 0, mono.data(), numSamples);
}

void FairoProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // True-bypass semantics: nothing touches the signal when bypassed.
}

void FairoProcessor::pushParametersToEngine()
{
    engine.setFuzz(apvts.getRawParameterValue("fuzz")->load());
    engine.setVolume(apvts.getRawParameterValue("volume")->load());
    engine.setTone(apvts.getRawParameterValue("tone")->load());
    engine.setHigh(apvts.getRawParameterValue("high")->load());

    engine.setHiLo(apvts.getRawParameterValue("hiLo")->load() > 0.5f);

    const int clip = static_cast<int>(apvts.getRawParameterValue("clipMode")->load());
    engine.setClipMode(clip == 0 ? fairo::FuzzEngine::ClipMode::Silicon
                       : clip == 1 ? fairo::FuzzEngine::ClipMode::Germanium
                                   : fairo::FuzzEngine::ClipMode::Bypass);

    // Phase 3 engine swap: Neural uses the trained GRU for the current branch;
    // falls back to DSP when the models are unavailable (still DSP-verified).
#if defined(FA_ENABLE_ONNX)
    const bool wantNeural = static_cast<int>(apvts.getRawParameterValue("engine")->load()) > 0
                            && neuralModelsLoaded;
    fairo::FuzzNeuralInference* active = nullptr;
    if (wantNeural)
    {
        active = (clip == 0) ? &neuralSi
               : (clip == 1) ? &neuralGe
                             : &neuralBy;
    }
    engine.setNeuralInference(active);
#else
    engine.setNeuralInference(nullptr);
#endif
}

juce::AudioProcessorEditor* FairoProcessor::createEditor()
{
    return new FairoEditor(*this);
}

void FairoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FairoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            apvts.replaceState(state);
            pushParametersToEngine();
        }
    }
}

// JUCE plugin entry point (required by every plugin client, notably AU).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FairoProcessor();
}
