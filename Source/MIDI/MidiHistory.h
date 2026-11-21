#pragma once
// EIDOLON — bounded event history and completed/active note records (engine thread only).
#include "../Core/MidiEvent.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eidolon
{
struct NoteRecord
{
    uint32_t id = 0;
    uint8_t  channel = 0, pitch = 0, velocity = 0;
    int8_t   voice = -1;
    double   on = 0.0;
    double   off = -1.0;        // < 0 while still sounding
    double duration (double now) const { return (off >= 0.0 ? off : now) - on; }
};

/** Fixed-capacity ring. Oldest entries are overwritten; never grows after construction. */
template <typename T>
class Ring
{
public:
    explicit Ring (size_t capacity) : data (capacity) {}
    void push (const T& v) { data[head % data.size()] = v; ++head; if (count < data.size()) ++count; }
    size_t size() const { return count; }
    size_t capacity() const { return data.size(); }
    /** i = 0 is the oldest retained element. */
    const T& operator[] (size_t i) const { return data[(head - count + i) % data.size()]; }
    T& at (size_t i) { return data[(head - count + i) % data.size()]; }
    const T& newest() const { return data[(head - 1) % data.size()]; }
    uint64_t totalPushed() const { return head; }
    void clear() { head = 0; count = 0; }

private:
    std::vector<T> data;
    uint64_t head = 0;
    size_t count = 0;
};

class MidiHistory
{
public:
    MidiHistory() : events (1u << 16), notes (8192) { openNotes.reserve (512); }

    void addEvent (const MidiEvent& e) { events.push (e); }

    void noteStarted (uint32_t id, int ch, int pitch, int vel, double t)
    {
        NoteRecord r; r.id = id; r.channel = (uint8_t) ch; r.pitch = (uint8_t) pitch; r.velocity = (uint8_t) vel; r.on = t;
        notes.push (r);
        openNotes[id] = notes.totalPushed() - 1;
    }

    void noteEnded (uint32_t id, double t)
    {
        auto it = openNotes.find (id);
        if (it == openNotes.end()) return;
        const uint64_t absIndex = it->second;
        openNotes.erase (it);
        const uint64_t oldestAbs = notes.totalPushed() - notes.size();
        if (absIndex < oldestAbs) return;     // record already overwritten
        notes.at ((size_t) (absIndex - oldestAbs)).off = t;
    }

    void setVoice (uint32_t id, int voice)
    {
        auto it = openNotes.find (id);
        if (it == openNotes.end()) return;
        const uint64_t oldestAbs = notes.totalPushed() - notes.size();
        if (it->second >= oldestAbs) notes.at ((size_t) (it->second - oldestAbs)).voice = (int8_t) voice;
    }

    void clear() { events.clear(); notes.clear(); openNotes.clear(); }

    Ring<MidiEvent> events;
    Ring<NoteRecord> notes;

private:
    std::unordered_map<uint32_t, uint64_t> openNotes;
};
} // namespace eidolon
