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
const juce::Colour kLabelDim(0xccb7a583);
const juce::Colour kAccent(0xffc8a24a);
const juce::Colour kAccentHi(0xffedcf86);
const juce::Colour kOutline(0x88c8a24a);
} // namespace

//==============================================================================
//  Bundled OFL typefaces
//  - Rajdhani  : wordmark, control labels, buttons (squared, legible, technical)
//  - Space Mono: numeric readouts + fine print (stable tabular digits)
//==============================================================================
namespace FairoFonts
{
namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    struct Holder
    {
        juce::Typeface::Ptr display;
        juce::Typeface::Ptr displayBold;
        juce::Typeface::Ptr mono;
        juce::Typeface::Ptr monoBold;

        Holder()
        {
            display     = load (BinaryData::RajdhaniMedium_ttf,   BinaryData::RajdhaniMedium_ttfSize);
            displayBold = load (BinaryData::RajdhaniBold_ttf,     BinaryData::RajdhaniBold_ttfSize);
            mono        = load (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
            monoBold    = load (BinaryData::SpaceMonoBold_ttf,    BinaryData::SpaceMonoBold_ttfSize);
        }

        static Holder& get()
        {
            static Holder instance;
            return instance;
        }
    };
} // namespace

static juce::Font display (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (bold ? Holder::get().displayBold : Holder::get().display)
                           .withHeight (height));
}

static juce::Font mono (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (bold ? Holder::get().monoBold : Holder::get().mono)
                           .withHeight (height));
}
} // namespace FairoFonts

namespace
{
// Lay a single line of text out at the origin with uniform letter-spacing (tracking).
juce::GlyphArrangement makeTrackedLayout (const juce::String& text, const juce::Font& font, float tracking)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);

    if (tracking > 0.0f)
    {
        const int n = ga.getNumGlyphs();
        for (int i = 1; i < n; ++i)
            ga.getGlyph (i).moveBy (tracking * (float) i, 0.0f);
    }

    return ga;
}

// Draw text with optional letter-spacing. Falls back to drawText() when untracked.
void drawTrackedText (juce::Graphics& g, const juce::String& text, const juce::Rectangle<float>& area,
                      juce::Justification justify, float tracking, const juce::Colour& colour,
                      const juce::Font& font)
{
    if (tracking <= 0.0f || text.length() < 2)
    {
        g.setColour (colour);
        g.setFont (font);
        g.drawText (text, area, justify);
        return;
    }

    auto ga = makeTrackedLayout (text, font, tracking);
    ga.justifyGlyphs (0, ga.getNumGlyphs(), area.getX(), area.getY(), area.getWidth(), area.getHeight(), justify);

    g.setColour (colour);
    g.setFont (font);
    ga.draw (g);
}

// Build the glyph outline path for a tracked string, positioned inside `area`.
juce::Path makeTrackedPath (const juce::String& text, const juce::Font& font, float tracking,
                            const juce::Rectangle<float>& area, juce::Justification justify)
{
    auto ga = makeTrackedLayout (text, font, tracking);
    ga.justifyGlyphs (0, ga.getNumGlyphs(), area.getX(), area.getY(), area.getWidth(), area.getHeight(), justify);

    juce::Path p;
    ga.createPath (p);
    return p;
}
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
    return FairoFonts::display(14.0f);
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

    const juce::Font font = FairoFonts::display(14.0f);
    const bool on = button.getToggleState();
    drawTrackedText(g, button.getButtonText(), bounds,
                    juce::Justification::centred, 0.8f,
                    on ? kAccentHi : kLabelCol, font);
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

    // ButtonAttachment already applied the persisted Hi/Lo value. Only sync the
    // label — calling setToggleState here fights the attachment and makes the
    // first click a no-op.
    hiLoButton.setClickingTogglesState(true);
    hiLoButton.onStateChange = [this]
    {
        hiLoButton.setButtonText(hiLoButton.getToggleState() ? "Hi" : "Lo");
    };
    hiLoButton.onStateChange();
    addAndMakeVisible(hiLoButton);

    // Add the items FIRST, then bind the attachment. Attaching to an empty
    // ComboBox makes attachParameter()'s initial sync a no-op, so the combo
    // would otherwise open on its hard-coded selection (the old bug: "Clip
    // reverts to Silicon on reopen"). With items present, the attachment reads
    // the saved clipMode value and restores it.
    clipModeBox.addItem("Silicon", 1);
    clipModeBox.addItem("Germanium", 2);
    clipModeBox.addItem("Bypass", 3);
    addAndMakeVisible(clipModeBox);
    clipAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getApvts(), "clipMode", clipModeBox);

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

    // ── Wordmark: tracked metal-sheen "FAIRO" over a soft shadow ──────────────
    const juce::Rectangle<float> word(0.0f, 14.0f, (float) getWidth(), 40.0f);
    const juce::Font titleFont = FairoFonts::display(29.0f, true);
    const float titleTracking = 29.0f * 0.055f;

    // soft drop shadow (two faint, offset passes read as a gentle blur)
    g.setColour(juce::Colour(0x66000000));
    g.fillPath(makeTrackedPath("FAIRO", titleFont, titleTracking, word.translated(0.0f, 2.0f),
                               juce::Justification::centred));
    g.setColour(juce::Colour(0x3d000000));
    g.fillPath(makeTrackedPath("FAIRO", titleFont, titleTracking, word.translated(0.0f, 1.0f),
                               juce::Justification::centred));

    // brushed-gold vertical sheen for the fill
    juce::ColourGradient sheen(juce::Colour(0xfff0d68e), word.getTopLeft(),
                               juce::Colour(0xff9a7329), word.getBottomLeft(), false);
    g.setGradientFill(sheen);
    g.fillPath(makeTrackedPath("FAIRO", titleFont, titleTracking, word, juce::Justification::centred));
    g.setColour(kAccent);

    // Version string (matches project(Fairo VERSION ...) in CMakeLists.txt).
    #ifndef FA_PLUGIN_VERSION
    #define FA_PLUGIN_VERSION "0.4.5"
    #endif
    const juce::Rectangle<float> versionRect(0.0f, (float) getHeight() - 24.0f,
                                             (float) getWidth(), 16.0f);
    drawTrackedText(g, juce::String("v") + juce::String(FA_PLUGIN_VERSION),
                    versionRect, juce::Justification::centred, 1.1f,
                    kLabelDim, FairoFonts::mono(10.5f));

    // ── Knob label + live numeric readout ─────────────────────────────────────
    auto drawKnob = [&](const juce::Slider& s, const juce::String& name)
    {
        const auto b = knobLabelBounds(s).toFloat();
        const float nameTracking = 1.2f;
        const juce::Font nameFont = FairoFonts::display(12.5f);

        drawTrackedText(g, name, b.translated(0.0f, 1.0f),
                        juce::Justification::centredTop, nameTracking,
                        juce::Colour(0xaa000000), nameFont);
        drawTrackedText(g, name, b, juce::Justification::centredTop,
                        nameTracking, kLabelCol, nameFont);

        const juce::String value = juce::String((float) s.getValue(), 2);
        drawTrackedText(g, value, b.translated(0.0f, -1.0f),
                        juce::Justification::centredBottom, 0.0f,
                        juce::Colour(0xaa000000), FairoFonts::mono(11.0f));
        drawTrackedText(g, value, b, juce::Justification::centredBottom, 0.0f,
                        kAccent, FairoFonts::mono(11.0f));
    };
    drawKnob(fuzzSlider, "Fuzz");
    drawKnob(toneSlider, "Tone");
    drawKnob(highSlider, "High");
    drawKnob(volumeSlider, "Volume");

    // ── Section labels above the switch row ───────────────────────────────────
    auto drawSectionLabel = [&](const juce::Rectangle<int>& owner, const juce::String& text)
    {
        const auto b = owner.toFloat().translated(0.0f, -24.0f);
        const juce::Font f = FairoFonts::display(12.5f);
        drawTrackedText(g, text, b.translated(0.0f, 1.0f),
                        juce::Justification::centred, 1.2f,
                        juce::Colour(0xaa000000), f);
        drawTrackedText(g, text, b, juce::Justification::centred,
                        1.2f, kLabelCol, f);
    };
    drawSectionLabel(hiLoButton.getBounds(), "Hi/Lo");
    drawSectionLabel(clipModeBox.getBounds(), "Clip");
}

juce::Rectangle<int> FairoEditor::knobLabelBounds(const juce::Slider& s) const
{
    return s.getBounds().translated(0, s.getHeight() + 4).withHeight(30);
}

void FairoEditor::resized()
{
    const int margin = 24;
    const int top = 82;
    const int knobSize = 88;

    fuzzSlider.setBounds(margin, top, knobSize, knobSize);
    volumeSlider.setBounds(getWidth() - margin - knobSize, top, knobSize, knobSize);
    toneSlider.setBounds(getWidth() / 2 - knobSize - 10, top, knobSize, knobSize);
    highSlider.setBounds(getWidth() / 2 + 10, top, knobSize, knobSize);

    const int hiLoW = 64;
    const int clipW = 120;
    const int rowGap = 16;
    const int groupW = hiLoW + rowGap + clipW;
    const int groupX = (getWidth() - groupW) / 2;
    const int rowY = top + knobSize + 108;
    hiLoButton.setBounds(groupX, rowY, hiLoW, 26);
    clipModeBox.setBounds(groupX + hiLoW + rowGap, rowY, clipW, 26);
}
