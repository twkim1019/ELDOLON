#pragma once
// EIDOLON — active-note tracking and note lifecycle (Active -> Sustained -> Released),
// per-channel controller state, stuck-note recovery. Runs on the engine (message) thread.
#include "../Core/MidiEvent.h"
#include <array>
#include <cstdint>
#include <vector>

namespace eidolon
{
enum class NoteState : uint8_t { Off, Active, Sustained };

/** Emitted for every lifecycle transition. Visual layers and analysis subscribe to these. */
struct NoteChange
{
    enum Kind : uint8_t { Start, KeyUp, Release };
    Kind     kind     = Start;
    uint32_t id       = 0;        // unique per note instance
    uint8_t  channel  = 0;
    uint8_t  pitch    = 0;
    uint8_t  velocity = 0;
    double   time     = 0.0;      // audio-clock time of the transition
    double   onTime   = 0.0;      // audio-clock time of the Note On
    bool     forced   = false;    // released by stuck-note recovery / panic / all-notes-off
};

struct NoteSlot
{
    NoteState state      = NoteState::Off;
    uint8_t   velocity   = 0;
    uint8_t   aftertouch = 0;
    double    onTime     = 0.0;
    uint32_t  id         = 0;
};

class NoteTracker
{
public:
    NoteTracker();

    /** Feeds one event; appends resulting lifecycle changes to `out`. */
    void process (const MidiEvent& e, std::vector<NoteChange>& out);

    /** Releases notes held longer than `timeout` seconds (0 disables). Returns number released. */
    int releaseStuck (double now, double timeout, std::vector<NoteChange>& out);

    /** Releases everything (panic). */
    void panic (double now, std::vector<NoteChange>& out);

    void setSustainAware (bool b) { sustainAware = b; }

    const NoteSlot& slot (int ch, int note) const { return slots[(size_t) (ch & 15)][(size_t) (note & 127)]; }
    int  activeCount() const      { return numActive; }      // Active + Sustained
    int  keyDownCount() const     { return numKeyDown; }     // Active only
    bool sustainOn (int ch) const { return sustain[(size_t) (ch & 15)]; }
    float pitchBend (int ch) const { return bend[(size_t) (ch & 15)]; }          // -1..1
    float pressure (int ch) const  { return chanPressure[(size_t) (ch & 15)]; }  // 0..1
    float cc (int ch, int n) const { return ccValues[(size_t) (ch & 15)][(size_t) (n & 127)]; } // 0..1
    int   program (int ch) const   { return programs[(size_t) (ch & 15)]; }
    uint16_t pitchClassMask (bool includeSustained) const;
    int lowestActive() const;   // -1 when none
    int highestActive() const;

    template <typename Fn> void forEachActive (Fn&& fn) const
    {
        for (int c = 0; c < 16; ++c)
            for (int n = 0; n < 128; ++n)
                if (slots[(size_t) c][(size_t) n].state != NoteState::Off) fn (c, n, slots[(size_t) c][(size_t) n]);
    }

private:
    void releaseNote (int ch, int note, double t, bool forced, std::vector<NoteChange>& out);
    void releaseChannel (int ch, double t, bool sustainedOnly, std::vector<NoteChange>& out);

    std::array<std::array<NoteSlot, 128>, 16> slots {};
    std::array<bool, 16> sustain {};
    std::array<float, 16> bend {};
    std::array<float, 16> chanPressure {};
    std::array<std::array<float, 128>, 16> ccValues {};
    std::array<int, 16> programs {};
    uint32_t nextId = 1;
    int numActive = 0, numKeyDown = 0;
    bool sustainAware = true;
};
} // namespace eidolon
