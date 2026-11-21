#include "VoiceTracker.h"
#include <algorithm>
#include <cmath>

namespace eidolon
{
int VoiceTracker::noteOn (uint32_t id, int channel, int pitch, double time)
{
    int chosen = 0;
    float bestCost = 1e9f, secondCost = 1e9f;

    switch (mode)
    {
        case VoiceMode::Channel:     chosen = channel % maxVoices; break;
        case VoiceMode::Monophonic:  chosen = 0; break;
        case VoiceMode::NoteSequence: chosen = roundRobin++ % maxVoices; break;
        case VoiceMode::PitchProximity:
        case VoiceMode::TemporalContinuity:
        case VoiceMode::Automatic:
        {
            for (int v = 0; v < maxVoices; ++v)
            {
                const auto& vc = voices[(size_t) v];
                float cost;
                const bool fresh = vc.lastPitch < 0;
                const double since = time - std::max (vc.lastOff, vc.lastOn);
                if (mode == VoiceMode::PitchProximity)
                    cost = (fresh ? 14.0f : (float) std::abs (pitch - vc.lastPitch)) + (vc.busy ? 40.0f : 0.0f);
                else if (mode == VoiceMode::TemporalContinuity)
                    cost = (vc.busy ? 60.0f : 0.0f) + (fresh ? 30.0f : (float) std::min (30.0, since * 8.0))
                         + (fresh ? 0.0f : 0.15f * (float) std::abs (pitch - vc.lastPitch));
                else
                {
                    const float pitchCost = fresh ? 9.0f : (float) std::abs (pitch - vc.lastPitch);
                    const float gapCost = fresh ? 0.0f : (float) std::min (12.0, since / std::max (0.05, gap) * 4.0);
                    // keep upper voices on top: penalize crossing below a voice's running mean
                    const float crossing = (! fresh && vc.noteCount > 2) ? 0.15f * std::abs ((float) pitch - vc.meanPitch) : 0.0f;
                    cost = pitchCost + gapCost + crossing + (vc.busy ? 50.0f : 0.0f);
                }
                if (cost < bestCost) { secondCost = bestCost; bestCost = cost; chosen = v; }
                else if (cost < secondCost) secondCost = cost;
            }
            break;
        }
    }

    if (mode == VoiceMode::PitchProximity || mode == VoiceMode::TemporalContinuity || mode == VoiceMode::Automatic)
    {
        const float amb = (secondCost - bestCost) < 2.0f ? 1.0f : 0.0f;
        ambiguityAvg = ambiguityAvg * 0.9f + amb * 0.1f;
    }
    else
    {
        ambiguityAvg *= 0.9f;
    }
    ++totalAssignments;

    auto& v = voices[(size_t) chosen];
    v.busy = true;
    v.noteId = id;
    v.lastPitch = pitch;
    v.lastOn = time;
    v.meanPitch = v.noteCount == 0 ? (float) pitch : v.meanPitch * 0.8f + (float) pitch * 0.2f;
    ++v.noteCount;
    return chosen;
}

void VoiceTracker::noteOff (uint32_t id, double time)
{
    for (auto& v : voices)
        if (v.busy && v.noteId == id) { v.busy = false; v.lastOff = time; }
}

int VoiceTracker::activeVoiceCount() const
{
    int n = 0;
    for (int v = 0; v < maxVoices; ++v) if (voices[(size_t) v].busy) ++n;
    return n;
}

void VoiceTracker::reset()
{
    voices = {};
    roundRobin = 0;
    ambiguityAvg = 0.0f;
    totalAssignments = 0;
}
} // namespace eidolon
