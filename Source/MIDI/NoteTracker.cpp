#include "NoteTracker.h"

namespace eidolon
{
NoteTracker::NoteTracker()
{
    for (auto& ch : ccValues) ch.fill (0.0f);
    for (auto& ch : ccValues) { ch[7] = 100.0f / 127.0f; ch[10] = 64.0f / 127.0f; ch[11] = 1.0f; }
}

void NoteTracker::releaseNote (int ch, int note, double t, bool forced, std::vector<NoteChange>& out)
{
    auto& s = slots[(size_t) ch][(size_t) note];
    if (s.state == NoteState::Off) return;
    if (s.state == NoteState::Active) --numKeyDown;
    --numActive;
    NoteChange c;
    c.kind = NoteChange::Release; c.id = s.id; c.channel = (uint8_t) ch; c.pitch = (uint8_t) note;
    c.velocity = s.velocity; c.time = t; c.onTime = s.onTime; c.forced = forced;
    out.push_back (c);
    s = NoteSlot {};
}

void NoteTracker::releaseChannel (int ch, double t, bool sustainedOnly, std::vector<NoteChange>& out)
{
    for (int n = 0; n < 128; ++n)
    {
        const auto st = slots[(size_t) ch][(size_t) n].state;
        if (st == NoteState::Sustained || (! sustainedOnly && st == NoteState::Active))
            releaseNote (ch, n, t, ! sustainedOnly, out);
    }
}

void NoteTracker::process (const MidiEvent& e, std::vector<NoteChange>& out)
{
    const int ch = e.channel & 15;
    switch (e.type)
    {
        case EvType::NoteOn:
        {
            if (e.data2 == 0) { MidiEvent off = e; off.type = EvType::NoteOff; process (off, out); return; }
            auto& s = slots[(size_t) ch][(size_t) (e.data1 & 127)];
            if (s.state != NoteState::Off) releaseNote (ch, e.data1 & 127, e.time, false, out);   // re-trigger
            s.state = NoteState::Active; s.velocity = e.data2; s.onTime = e.time; s.id = nextId++; s.aftertouch = 0;
            if (nextId == 0) nextId = 1;
            ++numActive; ++numKeyDown;
            NoteChange c;
            c.kind = NoteChange::Start; c.id = s.id; c.channel = (uint8_t) ch; c.pitch = e.data1 & 127;
            c.velocity = e.data2; c.time = e.time; c.onTime = e.time;
            out.push_back (c);
            break;
        }
        case EvType::NoteOff:
        {
            auto& s = slots[(size_t) ch][(size_t) (e.data1 & 127)];
            if (s.state != NoteState::Active) break;     // unmatched / already sustained: ignore
            if (sustainAware && sustain[(size_t) ch])
            {
                s.state = NoteState::Sustained;
                --numKeyDown;
                NoteChange c;
                c.kind = NoteChange::KeyUp; c.id = s.id; c.channel = (uint8_t) ch; c.pitch = e.data1 & 127;
                c.velocity = s.velocity; c.time = e.time; c.onTime = s.onTime;
                out.push_back (c);
            }
            else
            {
                releaseNote (ch, e.data1 & 127, e.time, false, out);
            }
            break;
        }
        case EvType::Controller:
        {
            const int n = e.data1 & 127;
            ccValues[(size_t) ch][(size_t) n] = (float) e.data2 / 127.0f;
            if (n == 64)
            {
                const bool down = e.data2 >= 64;
                if (sustain[(size_t) ch] && ! down) releaseChannel (ch, e.time, true, out);
                sustain[(size_t) ch] = down;
            }
            else if (n == 120 || n == 123)        // all sound off / all notes off
            {
                releaseChannel (ch, e.time, false, out);
                releaseChannel (ch, e.time, true, out);
            }
            else if (n == 121)                     // reset all controllers
            {
                if (sustain[(size_t) ch]) releaseChannel (ch, e.time, true, out);
                sustain[(size_t) ch] = false;
                bend[(size_t) ch] = 0.0f;
                chanPressure[(size_t) ch] = 0.0f;
            }
            break;
        }
        case EvType::PitchBend:       bend[(size_t) ch] = (float) e.value / (e.value >= 0 ? 8191.0f : 8192.0f); break;
        case EvType::ChannelPressure: chanPressure[(size_t) ch] = (float) e.data2 / 127.0f; break;
        case EvType::PolyAftertouch:
        {
            auto& s = slots[(size_t) ch][(size_t) (e.data1 & 127)];
            if (s.state != NoteState::Off) s.aftertouch = e.data2;
            break;
        }
        case EvType::ProgramChange:   programs[(size_t) ch] = e.data1 & 127; break;
        case EvType::Stop:
        case EvType::Clock:
        case EvType::Start:
        case EvType::Continue:
        case EvType::SongPosition:    break;
    }
}

int NoteTracker::releaseStuck (double now, double timeout, std::vector<NoteChange>& out)
{
    if (timeout <= 0.0 || numActive == 0) return 0;
    int released = 0;
    for (int c = 0; c < 16; ++c)
        for (int n = 0; n < 128; ++n)
        {
            const auto& s = slots[(size_t) c][(size_t) n];
            if (s.state != NoteState::Off && now - s.onTime > timeout) { releaseNote (c, n, now, true, out); ++released; }
        }
    return released;
}

void NoteTracker::panic (double now, std::vector<NoteChange>& out)
{
    for (int c = 0; c < 16; ++c)
    {
        for (int n = 0; n < 128; ++n) releaseNote (c, n, now, true, out);
        sustain[(size_t) c] = false;
        bend[(size_t) c] = 0.0f;
        chanPressure[(size_t) c] = 0.0f;
    }
}

uint16_t NoteTracker::pitchClassMask (bool includeSustained) const
{
    uint16_t m = 0;
    forEachActive ([&] (int, int n, const NoteSlot& s)
    {
        if (s.state == NoteState::Active || includeSustained) m |= (uint16_t) (1u << (n % 12));
    });
    return m;
}

int NoteTracker::lowestActive() const
{
    int lo = -1;
    forEachActive ([&] (int, int n, const NoteSlot&) { if (lo < 0 || n < lo) lo = n; });
    return lo;
}

int NoteTracker::highestActive() const
{
    int hi = -1;
    forEachActive ([&] (int, int n, const NoteSlot&) { if (n > hi) hi = n; });
    return hi;
}
} // namespace eidolon
