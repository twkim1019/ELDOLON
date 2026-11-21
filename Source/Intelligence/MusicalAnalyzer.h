#pragma once
// EIDOLON — real-time musical intelligence: aggregates chord, scale, voice, rhythm and statistics
// into one snapshot that the visual layers, the mapping engine and the dashboard read.
#include "ChordDetector.h"
#include "MusicalStatistics.h"
#include "RhythmAnalyzer.h"
#include "ScaleDetector.h"
#include "VoiceTracker.h"
#include "../Core/Params.h"
#include "../MIDI/NoteTracker.h"
#include <array>
#include <string>
#include <vector>

namespace eidolon
{
struct CCActivity { int channel = -1, number = 0, value = 0; double time = -1.0; };

struct AnalysisSnapshot
{
    double now = 0.0;
    int activeNotes = 0;           // sounding (incl. sustained)
    int keysDown = 0;
    int polyphony = 0;             // sounding notes (same as activeNotes; peak below)
    int peakPolyphony = 0;         // over the last 2 s
    int highest = -1, lowest = -1;
    float averageVelocity = 0.0f;  // of sounding notes (0..127)
    float recentVelocity = 0.0f;   // smoothed velocity of recent onsets
    float noteDensity = 0.0f;      // note-ons per second (smoothed)
    float registerCenter = 60.0f;  // smoothed mean pitch of recent notes
    float registerSpread = 12.0f;
    float meanDuration = 0.0f;     // seconds, window

    ChordInfo chord;
    bool chordChanged = false;
    float chordConfidenceRaw = 0.0f;
    int lastVoiceLeading = 0;
    int lastRootMotion = 0;
    std::vector<ChordHistoryEntry> chordHistory;

    ScaleInfo scale;
    RhythmInfo rhythm;

    float contour = 0.0f;                        // -1 descending .. +1 ascending (recent melodic slope)
    std::array<float, 64> contourLine {};        // last 64 melody pitches (top voice)
    int contourHead = 0;
    int lastInterval = 0;                        // last melodic interval (semitones, signed)

    std::array<float, 16> channelActivity {};    // decaying 0..1
    std::array<int, 16> channelNotes {};         // sounding notes per channel
    int lastChannel = -1;
    std::array<CCActivity, 8> recentCC {};
    std::array<bool, 16> sustain {};
    std::array<float, 16> pitchBend {};
    std::array<float, 16> pressure {};
    float maxPolyAftertouch = 0.0f;

    int voicesActive = 0;
    float voiceAmbiguity = 0.0f;

    uint64_t totalNotes = 0;
    uint64_t totalEvents = 0;
};

class MusicalAnalyzer
{
public:
    MusicalAnalyzer();

    void configure (const ParamSet& p);
    void onEvent (const MidiEvent& e);                                  // every received event
    int  onNoteChange (const NoteChange& c, const NoteTracker& tracker); // returns assigned voice for Start
    void update (double now, double dt, const NoteTracker& tracker, const TransportInfo& transport);
    void reset();

    const AnalysisSnapshot& snapshot() const { return snap; }
    const MidiHistory& history() const { return hist; }
    MidiHistory& history() { return hist; }
    const MusicalStatistics& statistics() const { return stats; }
    double statsWindowSeconds() const { return statsWindow; }

private:
    MidiHistory hist;
    ChordDetector chords;
    ScaleDetector scaleDet;
    VoiceTracker voices;
    RhythmAnalyzer rhythm;
    MusicalStatistics stats;
    AnalysisSnapshot snap;

    bool useSustainForChords = true;
    double statsWindow = 5.0;
    double lastStats = -1.0;
    std::array<double, 64> recentOnsets {};
    size_t onsetHead = 0;
    std::array<std::pair<double, int>, 64> polyHistory {};
    size_t polyHead = 0;
    int lastMelodyPitch = -1;
    double lastMelodyTime = -1.0;
    std::vector<int> pitchScratch;
};
} // namespace eidolon
