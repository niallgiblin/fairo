#pragma once

/**
 * @file
 * @brief Plugin editor: Fuzz/Volume/Tone/High rotaries + Hi/Lo and Clip-mode
 *        switch, over a gloomy amber coastal background (PLAN.md Phase 4).
 */

#include <JuceHeader.h>

class FairoProcessor;

/**
 * @brief Dark amber / desert-gold theme — semi-transparent controls over
 *        the background image, matching fuzzyband's frosted-panel pattern.
 */
class FairoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FairoLookAndFeel();
    ~FairoLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

class FairoEditor final : public juce::AudioProcessorEditor,
                          public juce::Slider::Listener
{
public:
    explicit FairoEditor(FairoProcessor&);
    ~FairoEditor() override;

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

    FairoLookAndFeel lookAndFeel;
    juce::Image backgroundImage;

    juce::AudioProcessorValueTreeState::SliderAttachment fuzzAttachment,
        volumeAttachment, toneAttachment, highAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment hiLoAttachment;
    // Created in the ctor body AFTER the combo items exist (see FairoEditor.cpp),
    // so attachParameter() can sync the initial selection from the saved state.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clipAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FairoEditor)
};
