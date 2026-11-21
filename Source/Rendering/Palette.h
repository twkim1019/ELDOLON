#pragma once
// EIDOLON — color palettes and musical color mapping.
#include "../Core/Params.h"
#include <juce_graphics/juce_graphics.h>

namespace eidolon
{
struct Palette
{
    juce::Colour stops[5];
    juce::Colour bgTop, bgBottom;
    float hueShift = 0.0f, saturation = 1.0f, brightness = 1.0f;

    /** t in [0,1] wraps around the palette gradient. */
    juce::Colour at (float t) const;
    /** Pitch class placed on the circle of fifths so harmonically close notes get close colors. */
    juce::Colour pitchClass (int pc) const { return at ((float) ((pc * 7) % 12) / 12.0f); }
    juce::Colour adjust (juce::Colour c) const;
};

Palette makePalette (const ParamSet& p);
const char* paletteName (int index);

/** Color for a note according to the 'Color by' setting. */
juce::Colour noteColour (const Palette& pal, int colorBy, int pitch, int channel, int velocity, int voice, int chordRoot);
} // namespace eidolon
