#include "MusicalStatistics.h"
#include <algorithm>
#include <cmath>

namespace eidolon
{
namespace
{
int durationBin (double d)
{
    if (d <= 0.0) return 0;
    const int b = (int) std::floor (std::log2 (d / 0.03125)) + 1;   // 31 ms, 62, 125, 250, 500 ms, 1, 2, 4, 8, 16 s
    return std::clamp (b, 0, 11);
}

void accumulate (Distributions& d, const NoteRecord& r, double now)
{
    d.pitch[r.pitch] += 1.0f;
    d.pitchClass[(size_t) (r.pitch % 12)] += 1.0f;
    d.octave[(size_t) std::clamp (r.pitch / 12, 0, 10)] += 1.0f;
    d.velocity[(size_t) std::min (15, r.velocity / 8)] += 1.0f;
    const double dur = r.duration (now);
    d.duration[(size_t) durationBin (dur)] += 1.0f;
    d.meanPitch += (float) r.pitch;
    d.meanVelocity += (float) r.velocity;
    d.meanDuration += (float) dur;
    d.lowest = d.lowest < 0 ? r.pitch : std::min (d.lowest, (int) r.pitch);
    d.highest = std::max (d.highest, (int) r.pitch);
    ++d.noteCount;
}

void finalise (Distributions& d)
{
    if (d.noteCount > 0)
    {
        d.meanPitch /= (float) d.noteCount;
        d.meanVelocity /= (float) d.noteCount;
        d.meanDuration /= (float) d.noteCount;
    }
    else
    {
        d.meanPitch = 60.0f;
    }
}
} // namespace

void MusicalStatistics::compute (const MidiHistory& history, double now, double window)
{
    dist = Distributions {};
    for (auto& col : heat) col.fill (0.0f);
    const auto& notes = history.notes;
    const bool instant = window <= 0.0;
    const double start = std::isinf (window) ? -1e300 : now - window;
    const double span = std::isinf (window) || instant ? 30.0 : window;

    for (size_t i = notes.size(); i-- > 0;)
    {
        const auto& r = notes[i];
        const bool sounding = r.off < 0.0;
        if (instant)
        {
            if (! sounding) continue;
        }
        else if ((sounding ? now : r.off) < start)
        {
            if (r.on < start - 60.0) break;   // records are ordered by onset; far older ones cannot overlap
            continue;
        }
        accumulate (dist, r, now);
        // heat columns: pitch-class presence per time slice of the window
        const double a = std::max (r.on, now - span), b = sounding ? now : r.off;
        if (b > now - span)
        {
            const int c0 = std::clamp ((int) ((a - (now - span)) / span * 48.0), 0, 47);
            const int c1 = std::clamp ((int) ((b - (now - span)) / span * 48.0), 0, 47);
            for (int c = c0; c <= c1; ++c) heat[(size_t) c][(size_t) (r.pitch % 12)] += (float) r.velocity / 127.0f;
        }
    }
    finalise (dist);
}

void MusicalStatistics::notePlayed (int pitch, int velocity)
{
    // running totals for "entire capture"; means are recomputed lazily from sums
    NoteRecord r; r.pitch = (uint8_t) pitch; r.velocity = (uint8_t) velocity; r.on = 0; r.off = 0;
    const float n = (float) total.noteCount;
    const float mp = total.meanPitch, mv = total.meanVelocity;
    total.meanPitch = 0; total.meanVelocity = 0; total.meanDuration = 0;
    accumulate (total, r, 0.0);
    total.meanPitch = (mp * n + (float) pitch) / (n + 1.0f);
    total.meanVelocity = (mv * n + (float) velocity) / (n + 1.0f);
}

void MusicalStatistics::sampleTimeline (double now, int polyphony, float density)
{
    if (lastSample >= 0.0 && now - lastSample < 0.125) return;
    lastSample = now;
    polyLine[(size_t) lineHead] = (float) polyphony;
    densLine[(size_t) lineHead] = density;
    lineHead = (lineHead + 1) % kTimelineLength;
}

void MusicalStatistics::reset()
{
    dist = total = Distributions {};
    polyLine.fill (0.0f);
    densLine.fill (0.0f);
    lineHead = 0;
    lastSample = -1.0;
}
} // namespace eidolon
