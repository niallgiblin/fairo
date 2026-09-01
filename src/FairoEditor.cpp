#include "FairoEditor.h"
#include "FairoProcessor.h"
#include <BinaryData.h>

namespace
{
const juce::Colour kPanelBg(0xff14100e);
const juce::Colour kPanelInset(0xcc1c1612);
const juce::Colour kKnobMetal(0xff3a332c);
const juce::Colour kKnobTip(0xffc8a24a);
const juce::Colour kLabelCol(0xffe8d8b8);
const juce::Colour kAccent(0xffc8a24a);
const juce::Colour kOutline(0x88c8a24a);
} // namespace

FairoLookAndFeel::FairoLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, kPanelInset);
    setColour(juce::ComboBox::textColourId, kLabelCol);
    setColour(juce::ComboBox::outlineColourId, kOutline);
    setColour(juce::ComboBox::focusedOutlineColourId, kAccent);
    setColour(juce::ComboBox::arrowColourId, kAccent);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xf0201a15));
    setColour(juce::PopupMenu::textColourId, kLabelCol);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff3d2f18));
    setColour(juce::PopupMenu::highlightedTextColourId, kAccent);
    setColour(juce::ToggleButton::textColourId, kLabelCol);
    setColour(juce::TextButton::buttonColourId, kPanelInset);
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xcc3d2f18));
    setColour(juce::TextButton::textColourOnId, kAccent);
    setColour(juce::TextButton::textColourOffId, kLabelCol);
    setColour(juce::Slider::textBoxTextColourId, kLabelCol);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void FairoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width,
                                        int height, float pos, float startAngle,
                                        float endAngle, juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const float radius = juce::jmin((float) width, (float) height) * 0.5f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> body(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Shadow so the metal disc reads on bright water highlights.
    g.setColour(juce::Colour(0x88000000));
    g.fillEllipse(body.translated(0.0f, 2.0f).expanded(1.5f));

    g.setColour(kKnobMetal);
    g.fillEllipse(body);
    g.setColour(juce::Colour(0xff59514a));
    g.drawEllipse(body.reduced(1.0f), 1.0f);

    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addTriangle(
        cx + 4.0f * std::cos(angle), cy + 4.0f * std::sin(angle),
        cx + (radius - 5.0f) * std::cos(angle - 0.09f), cy + (radius - 5.0f) * std::sin(angle - 0.09f),
        cx + (radius - 5.0f) * std::cos(angle + 0.09f), cy + (radius - 5.0f) * std::sin(angle + 0.09f));
    g.setColour(kKnobTip);
    g.fillPath(pointer);
}

juce::Font FairoLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::FontOptions(13.0f);
}

void FairoLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
    g.setColour(findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    const float arrowX = (float) width - 16.0f;
    const float arrowY = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY - 3.0f, arrowX + 7.0f, arrowY - 3.0f, arrowX + 3.5f, arrowY + 3.0f);
    g.setColour(findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void FairoLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, juce::jmax(1, box.getWidth() - 30), box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::textColourId, findColour(juce::ComboBox::textColourId));
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
}

void FairoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = findColour(button.getToggleState()
        ? juce::TextButton::buttonOnColourId
        : juce::TextButton::buttonColourId);
    if (isDown || isHighlighted)
        baseColour = baseColour.brighter(0.18f);
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(kOutline);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void FairoLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                        bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto base = button.getToggleState() ? juce::Colour(0xcc3d2f18) : kPanelInset;
    if (down || highlighted)
        base = base.brighter(0.18f);
    g.setColour(base);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(kOutline);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    g.setColour(button.getToggleState() ? kAccent : kLabelCol);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
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

    backgroundImage = juce::ImageCache::getFromMemory(
        BinaryData::coast_jpg, BinaryData::coast_jpgSize);

    for (auto* slider : { &fuzzSlider, &volumeSlider, &toneSlider, &highSlider })
    {
        slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider->setRange(0.0, 1.0);
        slider->addListener(this);   // keep the numeric readout live while dragging
        addAndMakeVisible(slider);
    }

    // Reflect the persisted Hi/Lo state (do NOT force it back to Lo on open).
    hiLoButton.setClickingTogglesState(true);
    const bool isHi = processor.getApvts().getRawParameterValue("hiLo")->load() > 0.5f;
    hiLoButton.setToggleState(isHi, juce::dontSendNotification);
    hiLoButton.setButtonText(isHi ? "Hi" : "Lo");
    hiLoButton.onClick = [this]
    {
        hiLoButton.setButtonText(hiLoButton.getToggleState() ? "Hi" : "Lo");
    };
    addAndMakeVisible(hiLoButton);

    clipModeBox.addItem("Silicon", 1);
    clipModeBox.addItem("Germanium", 2);
    clipModeBox.addItem("Bypass", 3);
    clipModeBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(clipModeBox);

    setSize(520, 360);
}

FairoEditor::~FairoEditor()
{
    setLookAndFeel(nullptr);
}

void FairoEditor::sliderValueChanged(juce::Slider*)
{
    repaint();  // the value readout is drawn in paint(); refresh it on every change
}

void FairoEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    if (backgroundImage.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(backgroundImage, bounds, juce::RectanglePlacement::fillDestination);
    }
    else
    {
        g.fillAll(kPanelBg);
    }

    // Dark warm wash — same idea as fuzzyband's forest vignette, in amber.
    g.setColour(juce::Colour(0x66080503));
    g.fillAll();

    juce::ColourGradient vig(
        juce::Colours::transparentBlack,
        bounds.getCentre(),
        juce::Colour(0x99050301),
        bounds.getCentre().translated(0.0f, bounds.getHeight() * 0.7f),
        true);
    g.setGradientFill(vig);
    g.fillRect(bounds);

    auto panel = bounds.reduced(10.0f);
    g.setColour(juce::Colour(0x5514100e));
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(kOutline);
    g.drawRoundedRectangle(panel, 14.0f, 1.5f);

    const auto title = juce::Rectangle<int>(0, 14, getWidth(), 34);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xaa000000));
    g.drawText("F A I R O", title.translated(1, 1), juce::Justification::centred);
    g.setColour(kAccent);
    g.drawText("F A I R O", title, juce::Justification::centred);

    // Version string (matches project(Fairo VERSION ...) in CMakeLists.txt).
    #ifndef FA_PLUGIN_VERSION
    #define FA_PLUGIN_VERSION "0.4.1"
    #endif
    g.setColour(kLabelCol);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String("v") + juce::String(FA_PLUGIN_VERSION),
               juce::Rectangle<int>(0, getHeight() - 28, getWidth(), 22),
               juce::Justification::centred);

    // Knob label + live numeric readout, so a knob at 1.0 is unmistakable.
    auto drawKnob = [&](const juce::Slider& s, const juce::String& name)
    {
        const auto b = knobLabelBounds(s);
        g.setColour(juce::Colour(0xaa000000));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(name, b.translated(1, 1), juce::Justification::centredTop);
        g.setColour(kLabelCol);
        g.drawText(name, b, juce::Justification::centredTop);
        g.setColour(kAccent);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String((float) s.getValue(), 2), b, juce::Justification::centredBottom);
    };
    drawKnob(fuzzSlider, "Fuzz");
    drawKnob(toneSlider, "Tone");
    drawKnob(highSlider, "High");
    drawKnob(volumeSlider, "Volume");

    g.setColour(kLabelCol);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Hi/Lo", hiLoButton.getBounds().translated(0, -24), juce::Justification::centred);
    g.drawText("Clip", clipModeBox.getBounds().translated(0, -24), juce::Justification::centred);
}

juce::Rectangle<int> FairoEditor::knobLabelBounds(const juce::Slider& s) const
{
    return s.getBounds().translated(0, s.getHeight() + 4).withHeight(30);
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

    hiLoButton.setBounds(24, top + knobSize + 66, 64, 26);
    clipModeBox.setBounds(104, top + knobSize + 66, 120, 26);
}
