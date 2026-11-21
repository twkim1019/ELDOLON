#pragma once
// EIDOLON — Standard MIDI File (type 0) writer for captured takes. JUCE-free so it is unit-testable.
#include "../Core/MidiEvent.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eidolon
{
struct SmfOptions
{
    int ticksPerQuarter = 960;
    double bpm = 120.0;          // tempo used to convert seconds to ticks (host tempo when known)
    int numerator = 4, denominator = 4;
    std::string trackName = "EIDOLON capture";
};

/** Builds SMF bytes. Event times are seconds relative to the take start. Hanging notes are closed at the end. */
std::vector<uint8_t> writeStandardMidiFile (const std::vector<MidiEvent>& events, double takeLength, const SmfOptions& opt);

/** Minimal reader used by the tests: returns note-on count, note-off count and total events parsed. */
struct SmfSummary { bool ok = false; int format = -1, tracks = 0, ppq = 0, noteOns = 0, noteOffs = 0, controllers = 0, pitchBends = 0; uint32_t lastTick = 0; };
SmfSummary summariseStandardMidiFile (const std::vector<uint8_t>& bytes);
} // namespace eidolon
