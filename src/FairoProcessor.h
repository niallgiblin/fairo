#pragma once

/**
 * @file
 * @brief JUCE AudioProcessor for Fairo (Pharaoh-style fuzz emulation).
 *
 * Parameter layout (PLAN.md Phase 0):
 *   fuzz, volume, tone, high  — 0..1 floats
 *   hiLo                     — Lo/Hi input switch (bool choice)
 *   clipMode                 — Silicon / Germanium / Bypass (enum choice)
 */

#include <JuceHeader.h>

#include "dsp/FuzzEngine.h"

class FairoProcessor final : public juce::AudioProcessor
{
public:
    FairoProcessor();
    ~FairoProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    static constexpr const char* kClipModeSilicon = "Silicon";
    static constexpr const char* kClipModeGermanium = "Germanium";
    static constexpr const char* kClipModeBypass = "Bypass";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void pushParametersToEngine();

    juce::AudioProcessorValueTreeState apvts;
    fairo::FuzzEngine engine;

    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FairoProcessor)
};
