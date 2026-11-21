#include "Palette.h"
#include "../Core/MathUtil.h"

namespace eidolon
{
namespace
{
struct Def { juce::uint32 c[5]; juce::uint32 top, bottom; const char* name; };
const Def kPalettes[] = {
    { { 0xff3ee6c1, 0xff4cc9f0, 0xff4f6bff, 0xff9b5cff, 0xfff25caf }, 0xff04060c, 0xff0b1222, "Aurora" },
    { { 0xffffd166, 0xfff4a261, 0xffe76f51, 0xffd64545, 0xffff8fa3 }, 0xff0a0504, 0xff1b0d08, "Ember" },
    { { 0xffcaf0f8, 0xff90e0ef, 0xff48cae4, 0xff3a9bdc, 0xff8fa3ff }, 0xff03070b, 0xff091724, "Glacier" },
    { { 0xffffffff, 0xffdbe2ea, 0xffb3bcc8, 0xff95a0ae, 0xffcfd8e3 }, 0xff050607, 0xff101215, "Monochrome" },
    { { 0xffff6b6e, 0xffffca3a, 0xff8ac926, 0xff35a7ff, 0xff9d7bff }, 0xff060609, 0xff111019, "Spectral" },
    { { 0xffc3c9ff, 0xff9d8cff, 0xff7a6cf0, 0xff5ccff5, 0xfff1e9da }, 0xff03030a, 0xff0d0b1f, "Nocturne" },
    { { 0xfffff3b0, 0xffffd23f, 0xffffa62b, 0xffee6c4d, 0xfff9e07a }, 0xff090602, 0xff1d1206, "Solar" },
    { { 0xffd8f3dc, 0xff95d5b2, 0xff52b788, 0xff3ab0a2, 0xffb7e4c7 }, 0xff020704, 0xff0a1710, "Verdant" },
    { { 0xffffe5ec, 0xffffb3c6, 0xffff8fab, 0xffc77dff, 0xff9d8df1 }, 0xff090407, 0xff1a0d16, "Rose Quartz" },
};
constexpr int kNum = (int) (sizeof (kPalettes) / sizeof (kPalettes[0]));
} // namespace

const char* paletteName (int index) { return kPalettes[clampv (index, 0, kNum - 1)].name; }

juce::Colour Palette::adjust (juce::Colour c) const
{
    float h, s, b;
    c.getHSB (h, s, b);
    h = std::fmod (h + hueShift + 1.0f, 1.0f);
    s = clampv (s * saturation, 0.0f, 1.0f);
    b = clampv (b * brightness, 0.0f, 1.0f);
    return juce::Colour::fromHSV (h, s, b, c.getFloatAlpha());
}

juce::Colour Palette::at (float t) const
{
    t = t - std::floor (t);
    const float f = t * 5.0f;
    const int i = (int) f % 5;
    const juce::Colour c = stops[i].interpolatedWith (stops[(i + 1) % 5], f - std::floor (f));
    return adjust (c);
}

Palette makePalette (const ParamSet& p)
{
    const auto& d = kPalettes[clampv (p.geti (P::LookPalette), 0, kNum - 1)];
    Palette pal;
    for (int i = 0; i < 5; ++i) pal.stops[i] = juce::Colour (d.c[i]);
    pal.hueShift = p.get (P::LookHue);
    pal.saturation = p.get (P::LookSaturation);
    pal.brightness = p.get (P::LookBrightness);
    const float tint = p.get (P::LookBackground);
    const auto base = juce::Colour (0xff030406);
    pal.bgTop = base.interpolatedWith (pal.adjust (juce::Colour (d.top)).withMultipliedBrightness (1.0f), 0.4f + 0.6f * tint);
    pal.bgBottom = base.interpolatedWith (pal.adjust (juce::Colour (d.bottom)), 0.3f + 0.7f * tint);
    return pal;
}

juce::Colour noteColour (const Palette& pal, int colorBy, int pitch, int channel, int velocity, int voice, int chordRoot)
{
    switch (colorBy)
    {
        case 1:  return pal.at ((float) (channel % 16) / 16.0f * 1.0f + 0.03f);
        case 2:  return pal.at (0.1f + 0.75f * (float) velocity / 127.0f);
        case 3:  return pal.at ((float) (pitch / 12) / 10.0f);
        case 4:  return pal.at ((float) (voice < 0 ? 0 : voice) / 8.0f * 1.6f);
        case 5:  return pal.pitchClass (chordRoot >= 0 ? chordRoot : pitch % 12).interpolatedWith (pal.pitchClass (pitch % 12), 0.35f);
        default: return pal.pitchClass (pitch % 12);
    }
}
} // namespace eidolon
