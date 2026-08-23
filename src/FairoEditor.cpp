#include "FairoEditor.h"
#include "FairoProcessor.h"

namespace
{
const juce::Colour kPanelBg(0xff14100e);
const juce::Colour kPanelInset(0xff241d18);
const juce::Colour kKnobMetal(0xff3a332c);
const juce::Colour kKnobTip(0xffc8a24a);
const juce::Colour kLabelCol(0xffcbb89a);
const juce::Colour kAccent(0xffc8a24a);
} // namespace

FairoLookAndFeel::FairoLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, kPanelBg);
    setColour(juce::ComboBox::backgroundColourId, kPanelInset);
    setColour(juce::ComboBox::textColourId, kLabelCol);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0x88c8a24a));
    setColour(juce::ComboBox::focusedOutlineColourId, kAccent);
    setColour(juce::ComboBox::arrowColourId, kAccent);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xf0201a15));
    setColour(juce::PopupMenu::textColourId, kLabelCol);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kPanelInset);
    setColour(juce::ToggleButton::textColourId, kLabelCol);
    setColour(juce::TextButton::buttonColourId, kPanelInset);
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3d2f18));
    setColour(juce::TextButton::textColourOnId, kAccent);
    setColour(juce::Slider::textBoxTextColourId, kLabelCol);
    setColour(juce::Slider::textBoxBackgroundColourId, kPanelInset);
}

void FairoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width,
                                        int height, float pos, float startAngle,
                                        float endAngle, juce::Slider& slider)
{
    const float radius = juce::jmin((float) width, (float) height) * 0.5f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> body(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Indicator arc
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour(kKnobMetal);
    g.fillEllipse(body);

    g.setColour(juce::Colour(0xff59514a) );
    g.drawEllipse(body.reduced(1.0f), 1.0f);

    // Knob pointer
    juce::Path pointer;
    pointer.addTriangle(
        cx + 4.0f * std::cos(angle) * 1.0f,
        cy + 4.0f * std::sin(angle) * 1.0f,
        cx + (radius - 5.0f) * std::cos(angle - 0.09f),
        cy + (radius - 5.0f) * std::sin(angle - 0.09f),
        cx + (radius - 5.0f) * std::cos(angle + 0.09f),
        cy + (radius - 5.0f) * std::sin(angle + 0.09f));
    g.setColour(kKnobTip);
    g.fillPath(pointer);

    g.setColour(juce::Colour(0x00000000));
    g.drawEllipse(body.reduced(radius * 0.35f), 1.0f);
}

FairoEditor::FairoEditor(FairoProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
    , lookAndFeel()
    , fuzzAttachment(processor.getApvts(), "fuzz", fuzzSlider)
    , volumeAttachment(processor.getApvts(), "volume", volumeSlider)
    , toneAttachment(processor.getApvts(), "tone", toneSlider)
    , highAttachment(processor.getApvts(), "high", highSlider)
    , hiLoAttachment(processor.getApvts(), "hiLo", hiLoButton)
    , clipAttachment(processor.getApvts(), "clipMode", clipModeBox)
{
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(false);

    for (auto* slider : { &fuzzSlider, &volumeSlider, &toneSlider, &highSlider })
    {
        slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider->setRange(0.0, 1.0);
        addAndMakeVisible(slider);
    }

    hiLoButton.setButtonText("Hi/Lo");
    hiLoButton.setClickingTogglesState(true);
    hiLoButton.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(hiLoButton);

    clipModeBox.addItem("Silicon", 1);
    clipModeBox.addItem("Germanium", 2);
    clipModeBox.addItem("Bypass", 3);
    clipModeBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(clipModeBox);

    setSize(520, 300);
}

void FairoEditor::paint(juce::Graphics& g)
{
    g.fillAll(kPanelBg);
    g.setColour(kPanelInset);
    g.fillRoundedRectangle(10.0f, 10.0f, (float) getWidth() - 20.0f, (float) getHeight() - 20.0f, 14.0f);
    g.setColour(juce::Colour(0xff0c0a08));
    g.drawRoundedRectangle(10.0f, 10.0f, (float) getWidth() - 20.0f, (float) getHeight() - 20.0f, 14.0f, 2.0f);

    g.setColour(kAccent);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("F A I R O", juce::Rectangle<int>(0, 14, getWidth(), 34), juce::Justification::centred);

    g.setColour(kLabelCol);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Fuzz", knobLabelBounds(fuzzSlider), juce::Justification::centred);
    g.drawText("Volume", knobLabelBounds(volumeSlider), juce::Justification::centred);
    g.drawText("Tone", knobLabelBounds(toneSlider), juce::Justification::centred);
    g.drawText("High", knobLabelBounds(highSlider), juce::Justification::centred);
    g.drawText("Clip", clipModeBox.getBounds().translated(0, -20), juce::Justification::centred);
}

juce::Rectangle<int> FairoEditor::knobLabelBounds(const juce::Slider& s) const
{
    return s.getBounds().translated(0, s.getHeight() + 4).withHeight(20);
}

void FairoEditor::resized()
{
    const int margin = 24;
    const int top = 58;
    const int knobSize = 88;

    fuzzSlider.setBounds(margin, top, knobSize, knobSize);
    volumeSlider.setBounds(getWidth() - margin - knobSize, top, knobSize, knobSize);
    toneSlider.setBounds(getWidth() / 2 - knobSize - 10, top, knobSize, knobSize);
    highSlider.setBounds(getWidth() / 2 + 10, top, knobSize, knobSize);

    hiLoButton.setBounds(getWidth() / 2 - 60, top + knobSize + 34, 56, 26);
    clipModeBox.setBounds(getWidth() / 2 + 4, top + knobSize + 34, 128, 26);
}
