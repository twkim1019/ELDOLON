#include "Theme.h"
#include "BinaryData.h"

namespace eidolon::ui
{
Fonts* Fonts::instance = nullptr;

Fonts& Fonts::get()
{
    if (instance == nullptr)
    {
        static juce::SharedResourcePointer<Fonts> fallback;   // defensive: an editor normally owns one
        return *fallback;
    }
    return *instance;
}

Fonts::Fonts()
{
    instance = this;
    lightFace   = juce::Typeface::createSystemTypefaceFor (BinaryData::InterLight_otf, BinaryData::InterLight_otfSize);
    regularFace = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
    mediumFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
    monoFace    = juce::Typeface::createSystemTypefaceFor (BinaryData::DejaVuSansMono_ttf, BinaryData::DejaVuSansMono_ttfSize);
}

Fonts::~Fonts()
{
    if (instance == this) instance = nullptr;
}

static juce::Font make (const juce::Typeface::Ptr& tf, float h)
{
    if (tf == nullptr) return juce::Font (juce::FontOptions (h));
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

juce::Font Fonts::light (float h) const   { return make (lightFace, h); }
juce::Font Fonts::regular (float h) const { return make (regularFace, h); }
juce::Font Fonts::medium (float h) const  { return make (mediumFace, h); }
juce::Font Fonts::mono (float h) const    { return make (monoFace, h); }
juce::Font Fonts::caps (float h, float tracking) const { return make (mediumFace, h).withExtraKerningFactor (tracking); }

// ------------------------------------------------------------------------------------------------- look and feel
EidolonLookAndFeel::EidolonLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg0);
    setColour (juce::PopupMenu::backgroundColourId, bg2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, bg3.brighter (0.15f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, bg3);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line2);
    setColour (juce::ComboBox::arrowColourId, text2);
    setColour (juce::Label::textColourId, text2);
    setColour (juce::TextButton::buttonColourId, bg3);
    setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.22f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, bg3);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, line2);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.25f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, line2);
    setColour (juce::TooltipWindow::backgroundColourId, bg2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
}

void EidolonLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle, juce::Slider& s)
{
    using namespace colours;
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cy = r.getCentreY();
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    g.setColour (bg3.brighter (0.08f));
    g.fillRoundedRectangle (r.getX(), cy - 1.5f, r.getWidth(), 3.0f, 1.5f);
    const float zeroX = bipolar ? r.getX() + r.getWidth() * (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : r.getX();
    const float x0 = std::min (zeroX, pos), x1 = std::max (zeroX, pos);
    const auto c = s.isEnabled() ? accent : text3;
    g.setColour (c.withAlpha (0.85f));
    g.fillRoundedRectangle (x0, cy - 1.5f, std::max (1.0f, x1 - x0), 3.0f, 1.5f);
    if (s.isMouseOverOrDragging())
    {
        g.setColour (c.withAlpha (0.18f));
        g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    }
    g.setColour (juce::Colour (0xfff2f5f8));
    g.fillEllipse (pos - 4.5f, cy - 4.5f, 9.0f, 9.0f);
}

void EidolonLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat();
    const float sw = 26.0f, sh = 14.0f;
    const auto sr = juce::Rectangle<float> (r.getRight() - sw - 2.0f, r.getCentreY() - sh * 0.5f, sw, sh);
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.35f) : bg3.brighter (highlighted ? 0.12f : 0.05f));
    g.fillRoundedRectangle (sr, sh * 0.5f);
    g.setColour (on ? accent : line2.brighter (0.2f));
    g.drawRoundedRectangle (sr, sh * 0.5f, 1.0f);
    const float kx = on ? sr.getRight() - sh + 2.0f : sr.getX() + 2.0f;
    g.setColour (on ? juce::Colours::white : text2);
    g.fillEllipse (kx, sr.getY() + 2.0f, sh - 4.0f, sh - 4.0f);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (b.isEnabled() ? text2 : text3);
        g.setFont (Fonts::get().regular (12.5f));
        g.drawText (b.getButtonText(), r.withTrimmedRight (sw + 8.0f), juce::Justification::centredLeft, true);
    }
}

void EidolonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? accent.withAlpha (0.16f) : bg3;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.brighter (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent.withAlpha (0.65f) : line2);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void EidolonLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : (b.isEnabled() ? colours::text : colours::text3));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
}

void EidolonLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? bg3.brighter (0.08f) : bg3);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent.withAlpha (0.6f) : line2);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (text2);
    g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EidolonLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font EidolonLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::get().regular (12.5f); }
juce::Font EidolonLookAndFeel::getPopupMenuFont() { return Fonts::get().regular (13.0f); }
juce::Font EidolonLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::get().medium (12.0f); }
juce::Font EidolonLookAndFeel::getLabelFont (juce::Label&) { return Fonts::get().regular (12.5f); }

void EidolonLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::bg2);
    g.setColour (colours::line2);
    g.drawRect (0, 0, w, h, 1);
}

void EidolonLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                        int thumbStart, int thumbSize, bool over, bool down)
{
    const auto thumb = vertical ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                                : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
    g.setColour (colours::line2.brighter (over || down ? 0.3f : 0.0f));
    g.fillRoundedRectangle (thumb, 2.0f);
}

void EidolonLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (colours::bg2);
    g.setColour (colours::line2);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (colours::text);
    g.setFont (Fonts::get().regular (12.5f));
    g.drawFittedText (text, juce::Rectangle<int> (0, 0, w, h).reduced (8, 5), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> EidolonLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = juce::jmin (320, 24 + (int) juce::GlyphArrangement::getStringWidth (Fonts::get().regular (12.5f), tip));
    const int lines = 1 + (int) (juce::GlyphArrangement::getStringWidth (Fonts::get().regular (12.5f), tip) / 300.0f);
    const int h = 12 + lines * 16;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 14, w, h).constrainedWithin (parent);
}

void EidolonLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::bg3);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void EidolonLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.6f) : colours::line2);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}

void drawSectionHeading (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
{
    g.setColour (colours::text3);
    g.setFont (Fonts::get().caps (10.5f));
    g.drawText (text.toUpperCase(), r, juce::Justification::centredLeft, false);
    const float tw = juce::GlyphArrangement::getStringWidth (Fonts::get().caps (10.5f), text.toUpperCase());
    g.setColour (colours::line);
    g.fillRect (r.getX() + tw + 10.0f, r.getCentreY(), r.getWidth() - tw - 10.0f, 1.0f);
}
} // namespace eidolon::ui
