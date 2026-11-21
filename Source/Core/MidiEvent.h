#pragma once
// EIDOLON — plain MIDI event record shared by the audio thread, the analysis engine, capture and export.
// POD on purpose: it is copied through a lock-free ring buffer from the audio callback.
#include <cstdint>

namespace eidolon
{
enum class EvType : uint8_t
{
    NoteOn, NoteOff, Controller, PitchBend, ChannelPressure, PolyAftertouch, ProgramChange,
    Clock, Start, Stop, Continue, SongPosition
};

struct MidiEvent
{
    double   time    = 0.0;   // seconds on the audio clock (sample accurate: block start + sample offset)
    EvType   type    = EvType::NoteOn;
    uint8_t  channel = 0;     // 0..15
    uint8_t  data1   = 0;     // note number / controller number / program
    uint8_t  data2   = 0;     // velocity / controller value / pressure
    int16_t  value   = 0;     // pitch bend -8192..8191, song position (16th notes)
    uint8_t  flags   = 0;     // bit 0: replayed internally from a capture (never re-captured)

    static constexpr uint8_t kReplayed = 1;

    bool isNote() const noexcept       { return type == EvType::NoteOn || type == EvType::NoteOff; }
    bool isContinuous() const noexcept
    {
        return type == EvType::Controller || type == EvType::PitchBend || type == EvType::ChannelPressure
            || type == EvType::PolyAftertouch;
    }
};

/** Host transport as last seen by the audio thread. */
struct TransportInfo
{
    bool   valid       = false;   // host provided tempo / position
    bool   playing     = false;
    double bpm         = 120.0;
    double ppq         = 0.0;     // quarter notes since song start
    double barStartPpq = 0.0;
    int    numerator   = 4;
    int    denominator = 4;
    double audioTime   = 0.0;     // audio clock time (seconds) at which ppq was sampled
};
} // namespace eidolon
