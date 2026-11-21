#pragma once
// EIDOLON — visual language: near-black graphite surfaces, restrained ice-blue / violet luminous accents,
// Inter typography with tracked uppercase labels. No neon, no gradients-for-their-own-sake.
#include <juce_gui_basics/juce_gui_basics.h>

namespace eidolon::ui
{
namespace colours
{
    inline const juce::Colour bg0     { 0xff060709 };
    inline const juce::Colour bg1     { 0xff0c0e11 };
    inline const juce::Colour bg2     { 0xff121519 };
    inline const juce::Colour bg3     { 0xff191d22 };
    inline const juce::Colour line    { 0xff1d2127 };
    inline const juce::Colour line2   { 0xff2a2f37 };
    inline const juce::Colour text    { 0xffe6e9ec };
    inline const juce::Colour text2   { 0xff9aa3ad };
    inline const juce::Colour text3   { 0xff5c646e };
    inline const juce::Colour accent  { 0xff9fd4ff };   // ice blue
    inline const juce::Colour accent2 { 0xffc4abff };   // soft violet
    inline const juce::Colour warm    { 0xffe8c48a };
    inline const juce::Colour ok      { 0xff86dcae };
    inline const juce::Colour warn    { 0xffe58a76 };
}

/** Embedded typefaces. Owned through juce::SharedResourcePointer by every editor. */
class Fonts
{
public:
    Fonts();
    ~Fonts();
    static Fonts& get();
    juce::Font light (float h) const;
    juce::Font regular (float h) const;
    juce::Font medium (float h) const;
    juce::Font mono (float h) const;
    juce::Font caps (float h, float tracking = 0.16f) const;

    juce::Typeface::Ptr lightFace, regularFace, mediumFace, monoFace;

private:
    static Fonts* instance;
    JUCE_DECLARE_NON_COPYABLE (Fonts)
};

class EidolonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EidolonLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                        int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 8; }
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

/** Small helper: draws an uppercase section heading with a hairline. */
void drawSectionHeading (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text);
} // namespace eidolon::ui
