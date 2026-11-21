#pragma once
// EIDOLON — chord recognition from pitch-class patterns, bass context and temporal stability.
// It reports "no chord" rather than forcing every note collection into a conventional chord.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace eidolon
{
struct ChordTemplate
{
    const char* typeName;   // "Major", "Minor 7", ...
    const char* suffix;     // "", "m", "maj7", ...
    uint16_t required;      // interval mask from the root (bit 0 = root)
    uint16_t optional;      // tones that may be present or omitted (e.g. the 5th of a 7th chord)
    float prior;            // small preference for common structures
};

struct ChordInfo
{
    bool valid = false;
    int root = -1;          // pitch class 0..11
    int bass = -1;          // pitch class of the lowest sounding note
    int templateIndex = -1; // into ChordDetector::templates(), -2 = custom, -3 = dyad
    int inversion = 0;      // 0 root position, 1 first, 2 second, 3 third, -1 slash (non-chord bass)
    float confidence = 0.0f;
    float score = 0.0f;
    uint16_t pitchClasses = 0;
    std::string name;       // "Cmaj7/E"
    std::string typeName;   // "Major 7"

    bool sameChordAs (const ChordInfo& o) const
    {
        return valid == o.valid && (! valid || (root == o.root && templateIndex == o.templateIndex && bass == o.bass));
    }
};

struct ChordHistoryEntry
{
    ChordInfo chord;
    double time = 0.0;
    int voiceLeadingSemitones = 0;   // total voice movement from the previous chord's voicing
    int rootMotion = 0;              // root movement in semitones (-6..+6)
};

struct ChordConfig
{
    double holdTime = 0.09;        // candidate must persist this long (anti-flicker)
    double releaseHold = 0.4;      // keep the last chord after notes stop
    int minNotes = 3;
    float minConfidence = 0.45f;
    uint16_t customTemplate = 0;   // optional user template (interval mask)
};

class ChordDetector
{
public:
    static const std::vector<ChordTemplate>& templates();
    static const char* pitchName (int pc, bool flats);

    void setConfig (const ChordConfig& c) { config = c; }
    void setPreferFlats (bool f) { preferFlats = f; }

    /** Stateless recognition. `weights` = salience per pitch class, `bassPc` = lowest note's class. */
    ChordInfo analyze (uint16_t pcMask, const std::array<float, 12>& weights, int bassPc) const;

    /** Stateful, temporally stabilized update. `pitches` = sounding MIDI notes (for voice leading). */
    void update (double now, uint16_t pcMask, const std::array<float, 12>& weights, int bassPc,
                 const std::vector<int>& pitches);

    const ChordInfo& current() const { return currentChord; }
    bool changed() const { return changedFlag; }           // true on the update that committed a new chord
    const std::vector<ChordHistoryEntry>& history() const { return hist; }   // newest last, max 32
    float rawConfidence() const { return lastCandidate.confidence; }

    static int voiceLeadingDistance (const std::vector<int>& a, const std::vector<int>& b);
    void reset();

private:
    std::string makeName (int root, int bass, const char* suffix, int inversion) const;

    ChordConfig config;
    bool preferFlats = false;
    ChordInfo currentChord, pendingChord, lastCandidate;
    double pendingSince = 0.0, lastValidTime = -1.0;
    bool changedFlag = false;
    std::vector<int> currentVoicing;
    std::vector<ChordHistoryEntry> hist;
};
} // namespace eidolon
