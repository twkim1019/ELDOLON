#include "ChordDetector.h"
#include "../Core/MathUtil.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace eidolon
{
namespace
{
constexpr uint16_t M (std::initializer_list<int> iv)
{
    uint16_t m = 0;
    for (int i : iv) m = (uint16_t) (m | (1u << (i % 12)));
    return m;
}
constexpr int kPowerIndex = 6;
} // namespace

const std::vector<ChordTemplate>& ChordDetector::templates()
{
    static const std::vector<ChordTemplate> t = {
        { "Major",              "",      M ({ 0, 4, 7 }),       0,               0.060f },
        { "Minor",              "m",     M ({ 0, 3, 7 }),       0,               0.060f },
        { "Diminished",         "dim",   M ({ 0, 3, 6 }),       0,               0.030f },
        { "Augmented",          "aug",   M ({ 0, 4, 8 }),       0,               0.020f },
        { "Sus2",               "sus2",  M ({ 0, 2, 7 }),       0,               0.020f },
        { "Sus4",               "sus4",  M ({ 0, 5, 7 }),       0,               0.025f },
        { "Power",              "5",     M ({ 0, 7 }),          0,               0.000f },   // dyads only
        { "Major 6",            "6",     M ({ 0, 4, 7, 9 }),    0,               0.030f },
        { "Minor 6",            "m6",    M ({ 0, 3, 7, 9 }),    0,               0.030f },
        { "Dominant 7",         "7",     M ({ 0, 4, 10 }),      M ({ 7 }),       0.045f },
        { "Major 7",            "maj7",  M ({ 0, 4, 11 }),      M ({ 7 }),       0.045f },
        { "Minor 7",            "m7",    M ({ 0, 3, 10 }),      M ({ 7 }),       0.045f },
        { "Minor-Major 7",      "mMaj7", M ({ 0, 3, 11 }),      M ({ 7 }),       0.030f },
        { "Diminished 7",       "dim7",  M ({ 0, 3, 6, 9 }),    0,               0.040f },
        { "Half-Diminished 7",  "m7b5",  M ({ 0, 3, 6, 10 }),   0,               0.040f },
        { "Dominant 7 sus4",    "7sus4", M ({ 0, 5, 10 }),      M ({ 7 }),       0.030f },
        { "Add 9",              "add9",  M ({ 0, 2, 4, 7 }),    0,               0.025f },
        { "Minor add 9",        "madd9", M ({ 0, 2, 3, 7 }),    0,               0.020f },
        { "6/9",                "6/9",   M ({ 0, 2, 4, 9 }),    M ({ 7 }),       0.020f },
        { "Dominant 9",         "9",     M ({ 0, 2, 4, 10 }),   M ({ 7 }),       0.030f },
        { "Major 9",            "maj9",  M ({ 0, 2, 4, 11 }),   M ({ 7 }),       0.030f },
        { "Minor 9",            "m9",    M ({ 0, 2, 3, 10 }),   M ({ 7 }),       0.030f },
        { "Dominant 7 b9",      "7b9",   M ({ 0, 1, 4, 10 }),   M ({ 7 }),       0.010f },
        { "Dominant 7 #9",      "7#9",   M ({ 0, 3, 4, 10 }),   M ({ 7 }),       0.010f },
        { "Dominant 11",        "11",    M ({ 0, 2, 5, 10 }),   M ({ 7, 4 }),    0.020f },
        { "Minor 11",           "m11",   M ({ 0, 3, 5, 10 }),   M ({ 2, 7 }),    0.020f },
        { "Dominant 13",        "13",    M ({ 0, 4, 9, 10 }),   M ({ 2, 7 }),    0.020f },
        { "Major 13",           "maj13", M ({ 0, 4, 9, 11 }),   M ({ 2, 7 }),    0.020f },
        { "Minor 13",           "m13",   M ({ 0, 3, 9, 10 }),   M ({ 2, 5, 7 }), 0.020f },
    };
    return t;
}

const char* ChordDetector::pitchName (int pc, bool flats)
{
    static const char* sharp[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flat[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return flats ? flat[pc] : sharp[pc];
}

std::string ChordDetector::makeName (int root, int bass, const char* suffix, int inversion) const
{
    std::string n = pitchName (root, preferFlats);
    n += suffix;
    if (inversion != 0 && bass >= 0 && bass != root) { n += "/"; n += pitchName (bass, preferFlats); }
    return n;
}

ChordInfo ChordDetector::analyze (uint16_t pcMask, const std::array<float, 12>& weights, int bassPc) const
{
    ChordInfo best;
    best.pitchClasses = pcMask;
    best.bass = bassPc;
    const int n = popcount12 (pcMask);
    if (n < std::max (2, config.minNotes)) return best;

    float total = 0.0f;
    for (int pc = 0; pc < 12; ++pc) if (pcMask & (1u << pc)) total += std::max (0.05f, weights[(size_t) pc]);
    if (total <= 0.0f) return best;

    const auto& T = templates();
    float bestScore = -1e9f, secondScore = -1e9f;
    int bestRoot = -1, bestT = -1;

    auto evaluate = [&] (int root, int tIndex, uint16_t req, uint16_t opt, float prior)
    {
        const uint16_t rel = rotateMask (pcMask, root);
        const int reqCount = popcount12 (req);
        const int miss = popcount12 ((uint16_t) (req & ~rel));
        if (miss > (reqCount >= 4 ? 1 : 0)) return;
        if (miss > 0 && (req & ~rel & M ({ 3, 4 }))) return;   // never drop the third that defines the quality
        const uint16_t covered = (uint16_t) (rel & (req | opt));
        const uint16_t extra = (uint16_t) (rel & ~(req | opt));
        float matchedW = 0.0f, extraW = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            const float w = std::max (0.05f, weights[(size_t) ((root + i) % 12)]);
            if (covered & (1u << i)) matchedW += w;
            else if (extra & (1u << i)) extraW += w;
        }
        float score = (matchedW - 1.25f * extraW) / total - 0.22f * (float) miss + prior
                    + (bassPc == root ? 0.10f : 0.0f) + 0.015f * (float) popcount12 (covered);
        if (score > bestScore)
        {
            secondScore = bestScore;
            bestScore = score; bestRoot = root; bestT = tIndex;
        }
        else if (score > secondScore)
        {
            secondScore = score;
        }
    };

    for (int root = 0; root < 12; ++root)
    {
        if (! (pcMask & (1u << root))) continue;           // the root must sound
        for (int t = 0; t < (int) T.size(); ++t)
        {
            if (t == kPowerIndex && n != 2) continue;
            evaluate (root, t, T[(size_t) t].required, T[(size_t) t].optional, T[(size_t) t].prior);
        }
        if (config.customTemplate != 0)
            evaluate (root, -2, (uint16_t) (config.customTemplate | 1u), 0, 0.05f);
    }

    if (bestRoot < 0)
    {
        if (n == 2 && config.minNotes <= 2)   // a plain interval: report it as such, not as a chord
        {
            int a = -1, b = -1;
            for (int pc = 0; pc < 12; ++pc) if (pcMask & (1u << pc)) { if (a < 0) a = pc; else b = pc; }
            if (bassPc == b) std::swap (a, b);
            static const char* iv[] = { "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7" };
            best.valid = true; best.root = a; best.templateIndex = -3; best.confidence = 0.5f;
            best.name = std::string (pitchName (a, preferFlats)) + "-" + pitchName (b, preferFlats);
            best.typeName = std::string ("Interval ") + iv[((b - a) % 12 + 12) % 12];
        }
        return best;
    }

    const float margin = saturate ((bestScore - secondScore) / 0.25f);
    best.confidence = saturate ((bestScore - 0.35f) / 0.75f) * (0.55f + 0.45f * margin);
    best.score = bestScore;
    if (best.confidence < config.minConfidence) return best;   // ambiguous collection: no forced label

    best.valid = true;
    best.root = bestRoot;
    best.templateIndex = bestT;
    const uint16_t req = bestT >= 0 ? T[(size_t) bestT].required : (uint16_t) (config.customTemplate | 1u);
    const uint16_t opt = bestT >= 0 ? T[(size_t) bestT].optional : 0;
    const int bi = bassPc >= 0 ? ((bassPc - bestRoot) % 12 + 12) % 12 : 0;
    if (bi == 0) best.inversion = 0;
    else if (! ((req | opt) & (1u << bi))) best.inversion = -1;                 // non-chord bass: slash chord
    else if (bi == 3 || bi == 4) best.inversion = 1;
    else if (bi >= 6 && bi <= 8) best.inversion = 2;
    else if (bi >= 9) best.inversion = 3;
    else best.inversion = -1;                                                   // 9th / 11th in the bass
    const char* suffix = bestT >= 0 ? T[(size_t) bestT].suffix : " (custom)";
    best.name = makeName (bestRoot, bassPc, suffix, best.inversion);
    best.typeName = bestT >= 0 ? T[(size_t) bestT].typeName : "Custom";
    return best;
}

int ChordDetector::voiceLeadingDistance (const std::vector<int>& a, const std::vector<int>& b)
{
    if (a.empty() || b.empty()) return 0;
    const auto& small = a.size() <= b.size() ? a : b;
    const auto& large = a.size() <= b.size() ? b : a;
    int total = 0;
    for (int p : small)
    {
        int bestD = 128;
        for (int q : large) bestD = std::min (bestD, std::abs (p - q));
        total += bestD;
    }
    return total;
}

void ChordDetector::update (double now, uint16_t pcMask, const std::array<float, 12>& weights, int bassPc,
                            const std::vector<int>& pitches)
{
    changedFlag = false;
    lastCandidate = analyze (pcMask, weights, bassPc);

    if (! lastCandidate.valid)
    {
        pendingChord = ChordInfo {};
        if (currentChord.valid && (lastValidTime < 0.0 || now - lastValidTime > config.releaseHold))
        {
            currentChord = ChordInfo {};
            changedFlag = true;
        }
        return;
    }

    lastValidTime = now;
    if (lastCandidate.sameChordAs (currentChord))
    {
        currentChord.confidence = currentChord.confidence * 0.85f + lastCandidate.confidence * 0.15f;
        currentVoicing = pitches;
        pendingChord = ChordInfo {};
        return;
    }

    if (! lastCandidate.sameChordAs (pendingChord))
    {
        pendingChord = lastCandidate;
        pendingSince = now;
    }

    const bool stableEnough = now - pendingSince >= config.holdTime;
    const bool strongEnough = ! currentChord.valid || lastCandidate.confidence >= currentChord.confidence * 0.7f;
    if (stableEnough && strongEnough)
    {
        ChordHistoryEntry h;
        h.chord = lastCandidate;
        h.time = now;
        h.voiceLeadingSemitones = voiceLeadingDistance (currentVoicing, pitches);
        if (! hist.empty() && hist.back().chord.valid)
        {
            int rm = ((lastCandidate.root - hist.back().chord.root) % 12 + 12) % 12;
            if (rm > 6) rm -= 12;
            h.rootMotion = rm;
        }
        hist.push_back (h);
        if (hist.size() > 32) hist.erase (hist.begin());
        currentChord = lastCandidate;
        currentVoicing = pitches;
        pendingChord = ChordInfo {};
        changedFlag = true;
    }
}

void ChordDetector::reset()
{
    currentChord = pendingChord = lastCandidate = ChordInfo {};
    hist.clear();
    currentVoicing.clear();
    lastValidTime = -1.0;
    changedFlag = false;
}
} // namespace eidolon
