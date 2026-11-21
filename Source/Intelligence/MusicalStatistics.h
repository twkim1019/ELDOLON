#pragma once
// EIDOLON — windowed MIDI statistics (pitch / pitch class / octave / velocity / duration distributions,
// polyphony and density timelines). This is MIDI analysis, not an audio FFT.
#include "../MIDI/MidiHistory.h"
#include <array>

namespace eidolon
{
struct Distributions
{
    std::array<float, 128> pitch {};
    std::array<float, 12>  pitchClass {};
    std::array<float, 11>  octave {};       // MIDI octave -1..9  -> index 0..10
    std::array<float, 16>  velocity {};     // 8-wide bins
    std::array<float, 12>  duration {};     // log bins: <31 ms .. > 16 s
    int noteCount = 0;
    float meanPitch = 60.0f, meanVelocity = 0.0f, meanDuration = 0.0f;
    int lowest = -1, highest = -1;
};

class MusicalStatistics
{
public:
    static constexpr int kTimelineLength = 240;   // 0.125 s steps = 30 s

    /** window <= 0 means "instant" (sounding notes only); infinite = entire capture since reset. */
    void compute (const MidiHistory& history, double now, double window);
    void sampleTimeline (double now, int polyphony, float density);
    void notePlayed (int pitch, int velocity);     // entire-capture accumulators

    const Distributions& windowed() const { return dist; }
    const Distributions& entire() const   { return total; }
    const std::array<float, kTimelineLength>& polyphonyTimeline() const { return polyLine; }
    const std::array<float, kTimelineLength>& densityTimeline() const   { return densLine; }
    const std::array<std::array<float, 12>, 48>& pitchClassHeat() const { return heat; }   // 48 columns over the window
    int timelineHead() const { return lineHead; }
    void reset();

private:
    Distributions dist, total;
    std::array<float, kTimelineLength> polyLine {}, densLine {};
    std::array<std::array<float, 12>, 48> heat {};
    int lineHead = 0;
    double lastSample = -1.0;
};
} // namespace eidolon
