#pragma once

/**
 * @file
 * @brief Plugin editor: Fuzz/Volume/Tone/High rotaries + Hi/Lo and Clip-mode
 *        switches, in a dark pedal-style skin (PLAN.md Phase 4).
 */

#include <JuceHeader.h>

class FairoProcessor;

class FairoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FairoLookAndFeel();
    ~FairoLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

class FairoEditor final : public juce::AudioProcessorEditor,
                          public juce::Slider::Listener
{
public:
    explicit FairoEditor(FairoProcessor&);
    ~FairoEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Live value redraw: keep the numeric readout in sync while dragging. */
    void sliderValueChanged(juce::Slider*) override;

private:
    FairoProcessor& processor;

    /** Label region directly below a knob. */
    juce::Rectangle<int> knobLabelBounds(const juce::Slider&) const;

    juce::Slider fuzzSlider, volumeSlider, toneSlider, highSlider;
    juce::ToggleButton hiLoButton;
    juce::ComboBox clipModeBox;
    juce::ComboBox engineBox;

    FairoLookAndFeel lookAndFeel;

    juce::AudioProcessorValueTreeState::SliderAttachment fuzzAttachment,
        volumeAttachment, toneAttachment, highAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment hiLoAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment clipAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment engineAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FairoEditor)
};
