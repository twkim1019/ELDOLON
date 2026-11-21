#pragma once
// EIDOLON — onset rate, rhythmic density, tempo and beat / bar phase.
// Beat source priority: host transport > MIDI clock > estimate from note onsets.
#include "../Core/MidiEvent.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace eidolon
{
enum class BeatSource : int { None, Host, MidiClock, Estimated };

struct RhythmInfo
{
    float onsetRate = 0.0f;        // onsets per second (2 s window)
    float density = 0.0f;          // normalized 0..1 rhythmic density
    float bpm = 0.0f;
    float beatPhase = 0.0f;        // 0..1 within the beat
    float barPhase = 0.0f;         // 0..1 within the bar
    int   beatInBar = 0;
    int   beatsPerBar = 4;
    BeatSource source = BeatSource::None;
    bool  beatTick = false;        // a beat boundary was crossed during this update
    bool  barTick = false;
    float regularity = 0.0f;       // 0..1, how periodic the onsets are
};

class RhythmAnalyzer
{
public:
    void onset (double time, int velocity);
    void midiClock (const MidiEvent& e);     // Clock / Start / Stop / Continue / SongPosition
    void update (double now, const TransportInfo& transport);
    const RhythmInfo& info() const { return state; }
    void reset();

private:
    float estimateTempo (double now);

    std::array<double, 256> onsets {};
    size_t onsetHead = 0, onsetCount = 0;
    double clockTicks = 0.0;
    double lastClockTime = -1.0, clockInterval = 0.0;
    bool clockRunning = false;
    double estPhaseAnchor = 0.0;
    float estBpm = 0.0f;
    double lastEstimate = -1.0;
    double lastBeatIndex = -1.0, lastBarIndex = -1.0;
    RhythmInfo state;
};
} // namespace eidolon
