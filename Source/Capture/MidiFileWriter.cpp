#include "MidiFileWriter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace eidolon
{
namespace
{
void put32 (std::vector<uint8_t>& b, uint32_t v) { for (int s = 24; s >= 0; s -= 8) b.push_back ((uint8_t) (v >> s)); }
void put16 (std::vector<uint8_t>& b, uint16_t v) { b.push_back ((uint8_t) (v >> 8)); b.push_back ((uint8_t) v); }
void putVlq (std::vector<uint8_t>& b, uint32_t v)
{
    uint8_t tmp[5]; int n = 0;
    tmp[n++] = (uint8_t) (v & 0x7f);
    while ((v >>= 7) != 0) tmp[n++] = (uint8_t) ((v & 0x7f) | 0x80);
    while (n-- > 0) b.push_back (tmp[n]);
}

struct TickEvent { uint32_t tick; int order; std::array<uint8_t, 3> bytes; int len; };
} // namespace

std::vector<uint8_t> writeStandardMidiFile (const std::vector<MidiEvent>& events, double takeLength, const SmfOptions& opt)
{
    const double ticksPerSecond = opt.ticksPerQuarter * opt.bpm / 60.0;
    auto toTick = [&] (double t) { return (uint32_t) std::max (0.0, std::llround (t * ticksPerSecond) * 1.0); };

    std::vector<TickEvent> te;
    te.reserve (events.size() + 64);
    std::array<std::array<bool, 128>, 16> open {};
    int order = 0;

    for (const auto& e : events)
    {
        const uint8_t ch = e.channel & 15;
        TickEvent x { toTick (e.time), order++, { 0, 0, 0 }, 3 };
        switch (e.type)
        {
            case EvType::NoteOn:
                if (e.data2 == 0) { x.bytes = { (uint8_t) (0x80 | ch), (uint8_t) (e.data1 & 127), 0 }; open[ch][e.data1 & 127] = false; }
                else
                {
                    if (open[ch][e.data1 & 127])   // re-trigger: close the previous instance first
                        te.push_back ({ x.tick, order++, { (uint8_t) (0x80 | ch), (uint8_t) (e.data1 & 127), 0 }, 3 });
                    x.bytes = { (uint8_t) (0x90 | ch), (uint8_t) (e.data1 & 127), (uint8_t) (e.data2 & 127) };
                    open[ch][e.data1 & 127] = true;
                }
                break;
            case EvType::NoteOff:
                if (! open[ch][e.data1 & 127]) continue;
                x.bytes = { (uint8_t) (0x80 | ch), (uint8_t) (e.data1 & 127), (uint8_t) (e.data2 & 127) };
                open[ch][e.data1 & 127] = false;
                break;
            case EvType::Controller:      x.bytes = { (uint8_t) (0xB0 | ch), (uint8_t) (e.data1 & 127), (uint8_t) (e.data2 & 127) }; break;
            case EvType::ProgramChange:   x.bytes = { (uint8_t) (0xC0 | ch), (uint8_t) (e.data1 & 127), 0 }; x.len = 2; break;
            case EvType::ChannelPressure: x.bytes = { (uint8_t) (0xD0 | ch), (uint8_t) (e.data2 & 127), 0 }; x.len = 2; break;
            case EvType::PolyAftertouch:  x.bytes = { (uint8_t) (0xA0 | ch), (uint8_t) (e.data1 & 127), (uint8_t) (e.data2 & 127) }; break;
            case EvType::PitchBend:
            {
                const int v = std::clamp ((int) e.value + 8192, 0, 16383);
                x.bytes = { (uint8_t) (0xE0 | ch), (uint8_t) (v & 0x7f), (uint8_t) ((v >> 7) & 0x7f) };
                break;
            }
            default: continue;   // clock / transport messages are not written to the file
        }
        te.push_back (x);
    }

    const uint32_t endTick = std::max (toTick (takeLength), te.empty() ? 0u : std::max_element (te.begin(), te.end(),
                                       [] (const TickEvent& a, const TickEvent& b) { return a.tick < b.tick; })->tick);
    for (int c = 0; c < 16; ++c)
        for (int n = 0; n < 128; ++n)
            if (open[(size_t) c][(size_t) n]) te.push_back ({ endTick, order++, { (uint8_t) (0x80 | c), (uint8_t) n, 0 }, 3 });

    std::stable_sort (te.begin(), te.end(), [] (const TickEvent& a, const TickEvent& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    std::vector<uint8_t> track;
    // track name
    putVlq (track, 0); track.push_back (0xFF); track.push_back (0x03);
    putVlq (track, (uint32_t) opt.trackName.size());
    track.insert (track.end(), opt.trackName.begin(), opt.trackName.end());
    // tempo
    const uint32_t uspq = (uint32_t) std::llround (60000000.0 / std::max (1.0, opt.bpm));
    putVlq (track, 0); track.push_back (0xFF); track.push_back (0x51); track.push_back (3);
    track.push_back ((uint8_t) (uspq >> 16)); track.push_back ((uint8_t) (uspq >> 8)); track.push_back ((uint8_t) uspq);
    // time signature
    int dd = 0; for (int d = std::max (1, opt.denominator); d > 1; d >>= 1) ++dd;
    putVlq (track, 0); track.push_back (0xFF); track.push_back (0x58); track.push_back (4);
    track.push_back ((uint8_t) opt.numerator); track.push_back ((uint8_t) dd); track.push_back (24); track.push_back (8);

    uint32_t prev = 0;
    for (const auto& x : te)
    {
        putVlq (track, x.tick - prev);
        prev = x.tick;
        for (int i = 0; i < x.len; ++i) track.push_back (x.bytes[(size_t) i]);
    }
    putVlq (track, endTick > prev ? endTick - prev : 0);
    track.push_back (0xFF); track.push_back (0x2F); track.push_back (0x00);

    std::vector<uint8_t> out;
    out.insert (out.end(), { 'M', 'T', 'h', 'd' });
    put32 (out, 6); put16 (out, 0); put16 (out, 1); put16 (out, (uint16_t) opt.ticksPerQuarter);
    out.insert (out.end(), { 'M', 'T', 'r', 'k' });
    put32 (out, (uint32_t) track.size());
    out.insert (out.end(), track.begin(), track.end());
    return out;
}

SmfSummary summariseStandardMidiFile (const std::vector<uint8_t>& b)
{
    SmfSummary s;
    auto rd32 = [&] (size_t p) { return (uint32_t) b[p] << 24 | (uint32_t) b[p + 1] << 16 | (uint32_t) b[p + 2] << 8 | b[p + 3]; };
    auto rd16 = [&] (size_t p) { return (uint16_t) (b[p] << 8 | b[p + 1]); };
    if (b.size() < 14 || b[0] != 'M' || b[1] != 'T' || b[2] != 'h' || b[3] != 'd') return s;
    s.format = rd16 (8); s.tracks = rd16 (10); s.ppq = rd16 (12);
    size_t p = 14;
    for (int t = 0; t < s.tracks; ++t)
    {
        if (p + 8 > b.size() || b[p] != 'M' || b[p + 1] != 'T' || b[p + 2] != 'r' || b[p + 3] != 'k') return s;
        const size_t len = rd32 (p + 4), end = p + 8 + len;
        if (end > b.size()) return s;
        p += 8;
        uint32_t tick = 0; uint8_t running = 0;
        while (p < end)
        {
            uint32_t delta = 0; uint8_t c;
            do { c = b[p++]; delta = (delta << 7) | (c & 0x7f); } while ((c & 0x80) && p < end);
            tick += delta;
            uint8_t st = b[p];
            if (st & 0x80) ++p; else st = running;
            if (st == 0xFF)
            {
                const uint8_t type = b[p++];
                uint32_t l = 0; do { c = b[p++]; l = (l << 7) | (c & 0x7f); } while (c & 0x80);
                p += l;
                if (type == 0x2F) break;
                continue;
            }
            running = st;
            const uint8_t hi = st & 0xF0;
            const int n = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
            const uint8_t d2 = n == 2 ? b[p + 1] : 0;
            if (hi == 0x90 && d2 > 0) ++s.noteOns;
            else if (hi == 0x80 || (hi == 0x90 && d2 == 0)) ++s.noteOffs;
            else if (hi == 0xB0) ++s.controllers;
            else if (hi == 0xE0) ++s.pitchBends;
            p += (size_t) n;
        }
        s.lastTick = std::max (s.lastTick, tick);
        p = end;
    }
    s.ok = true;
    return s;
}
} // namespace eidolon
