#pragma once
// EIDOLON — configurable polyphonic voice separation.
// MIDI rarely encodes voices explicitly; the heuristic modes are estimates and report their own ambiguity.
#include <array>
#include <cstddef>
#include <cstdint>

namespace eidolon
{
enum class VoiceMode : int { Channel, Monophonic, PitchProximity, TemporalContinuity, NoteSequence, Automatic };

class VoiceTracker
{
public:
    static constexpr int kMaxVoices = 8;

    void setMode (VoiceMode m) { mode = m; }
    void setMaxVoices (int n) { maxVoices = n < 1 ? 1 : (n > kMaxVoices ? kMaxVoices : n); }
    void setContinuityGap (double s) { gap = s; }

    /** Assigns a voice to a starting note. */
    int noteOn (uint32_t id, int channel, int pitch, double time);
    void noteOff (uint32_t id, double time);

    struct Voice
    {
        bool  busy = false;
        uint32_t noteId = 0;
        int   lastPitch = -1;
        double lastOn = -1e9, lastOff = -1e9;
        float meanPitch = 60.0f;
        int   noteCount = 0;
    };
    const Voice& voice (int i) const { return voices[(size_t) (i & (kMaxVoices - 1))]; }
    int activeVoiceCount() const;

    /** Share of recent assignments where the best and second-best voice were nearly equally plausible. */
    float ambiguity() const { return ambiguityAvg; }
    int assignments() const { return totalAssignments; }
    void reset();

private:
    std::array<Voice, kMaxVoices> voices {};
    VoiceMode mode = VoiceMode::Automatic;
    int maxVoices = 4;
    double gap = 1.0;
    int roundRobin = 0;
    float ambiguityAvg = 0.0f;
    int totalAssignments = 0;
};
} // namespace eidolon
