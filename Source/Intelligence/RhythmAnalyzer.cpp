#include "RhythmAnalyzer.h"
#include "../Core/MathUtil.h"
#include <algorithm>
#include <cmath>

namespace eidolon
{
void RhythmAnalyzer::onset (double time, int velocity)
{
    (void) velocity;
    // collapse near-simultaneous onsets (chords) into one rhythmic event
    if (onsetCount > 0)
    {
        const double last = onsets[(onsetHead + onsets.size() - 1) % onsets.size()];
        if (time - last < 0.03) return;
    }
    onsets[onsetHead] = time;
    onsetHead = (onsetHead + 1) % onsets.size();
    if (onsetCount < onsets.size()) ++onsetCount;
    if (estBpm > 0.0f)
    {
        // re-anchor the estimated phase gently toward strong onsets
        const double period = 60.0 / estBpm;
        const double ph = std::fmod (time - estPhaseAnchor, period) / period;
        const double err = ph > 0.5 ? ph - 1.0 : ph;
        estPhaseAnchor += err * period * 0.25;
    }
    else
    {
        estPhaseAnchor = time;
    }
}

void RhythmAnalyzer::midiClock (const MidiEvent& e)
{
    switch (e.type)
    {
        case EvType::Start:        clockTicks = 0; clockRunning = true; break;
        case EvType::Continue:     clockRunning = true; break;
        case EvType::Stop:         clockRunning = false; break;
        case EvType::SongPosition: clockTicks = (double) e.value * 6.0; break;
        case EvType::Clock:
            if (lastClockTime >= 0.0)
            {
                const double dt = e.time - lastClockTime;
                if (dt > 0.0 && dt < 0.25) clockInterval = clockInterval <= 0.0 ? dt : clockInterval * 0.9 + dt * 0.1;
            }
            lastClockTime = e.time;
            clockRunning = true;
            clockTicks += 1.0;
            break;
        default: break;
    }
}

float RhythmAnalyzer::estimateTempo (double now)
{
    // vote inter-onset intervals (and their multiples) into a tempo histogram folded to 60..180 BPM
    std::array<float, 121> votes {};
    int used = 0;
    for (size_t i = 0; i < onsetCount; ++i)
    {
        const double a = onsets[(onsetHead + onsets.size() - 1 - i) % onsets.size()];
        if (now - a > 8.0) break;
        for (size_t j = i + 1; j < std::min (onsetCount, i + 6); ++j)
        {
            const double b = onsets[(onsetHead + onsets.size() - 1 - j) % onsets.size()];
            double ioi = a - b;
            if (ioi < 0.15 || ioi > 2.5) continue;
            double bpm = 60.0 / ioi;
            while (bpm < 60.0) bpm *= 2.0;
            while (bpm > 180.0) bpm *= 0.5;
            const int k = (int) std::lround (bpm - 60.0);
            for (int d = -2; d <= 2; ++d)
            {
                const int idx = k + d;
                if (idx >= 0 && idx < (int) votes.size()) votes[(size_t) idx] += std::exp (-0.5f * (float) (d * d)) / (float) (j - i);
            }
            ++used;
        }
    }
    if (used < 6) return 0.0f;
    int best = 0;
    float total = 0;
    for (int i = 0; i < (int) votes.size(); ++i) { total += votes[(size_t) i]; if (votes[(size_t) i] > votes[(size_t) best]) best = i; }
    state.regularity = total > 0 ? saturate (votes[(size_t) best] / total * 6.0f) : 0.0f;
    return 60.0f + (float) best;
}

void RhythmAnalyzer::update (double now, const TransportInfo& tr)
{
    int recent = 0;
    for (size_t i = 0; i < onsetCount; ++i)
    {
        const double t = onsets[(onsetHead + onsets.size() - 1 - i) % onsets.size()];
        if (now - t > 2.0) break;
        ++recent;
    }
    const float rate = (float) recent / 2.0f;
    state.onsetRate = state.onsetRate * 0.8f + rate * 0.2f;
    state.density = saturate (state.onsetRate / 12.0f);

    if (lastEstimate < 0.0 || now - lastEstimate > 0.5)
    {
        const float b = estimateTempo (now);
        if (b > 0.0f) estBpm = estBpm <= 0.0f ? b : estBpm * 0.7f + b * 0.3f;
        lastEstimate = now;
    }

    double beatPos = -1.0;   // continuous beat count
    state.beatsPerBar = 4;
    if (tr.valid && tr.playing)
    {
        state.source = BeatSource::Host;
        state.bpm = (float) tr.bpm;
        const double extrapolated = tr.ppq + std::max (0.0, now - tr.audioTime) * tr.bpm / 60.0;
        const double quarterPerBeat = 4.0 / std::max (1, tr.denominator);
        beatPos = extrapolated / quarterPerBeat;
        state.beatsPerBar = std::max (1, tr.numerator);
        const double barLen = state.beatsPerBar * quarterPerBeat;
        const double sinceBar = extrapolated - tr.barStartPpq;
        state.barPhase = (float) (std::fmod (std::max (0.0, sinceBar), barLen) / barLen);
    }
    else if (clockRunning && lastClockTime >= 0.0 && now - lastClockTime < 0.5 && clockInterval > 0.0)
    {
        state.source = BeatSource::MidiClock;
        state.bpm = (float) (60.0 / (clockInterval * 24.0));
        beatPos = clockTicks / 24.0;
        state.barPhase = (float) (std::fmod (beatPos, 4.0) / 4.0);
    }
    else if (estBpm > 0.0f && recent > 2)
    {
        state.source = BeatSource::Estimated;
        state.bpm = estBpm;
        beatPos = (now - estPhaseAnchor) * estBpm / 60.0;
        state.barPhase = (float) (std::fmod (std::fmod (beatPos, 4.0) + 4.0, 4.0) / 4.0);
    }
    else
    {
        state.source = BeatSource::None;
    }

    state.beatTick = state.barTick = false;
    if (beatPos >= 0.0 || state.source == BeatSource::Estimated)
    {
        const double beatIndex = std::floor (beatPos);
        state.beatPhase = (float) (beatPos - beatIndex);
        state.beatInBar = (int) (state.barPhase * (float) state.beatsPerBar);
        if (lastBeatIndex >= 0.0 && beatIndex != lastBeatIndex) state.beatTick = true;
        const double barIndex = std::floor (beatPos / state.beatsPerBar);
        if (state.beatTick && lastBarIndex >= 0.0 && barIndex != lastBarIndex) state.barTick = true;
        lastBeatIndex = beatIndex;
        lastBarIndex = barIndex;
    }
    else
    {
        lastBeatIndex = lastBarIndex = -1.0;
    }
}

void RhythmAnalyzer::reset()
{
    onsetHead = onsetCount = 0;
    estBpm = 0.0f;
    clockRunning = false;
    lastClockTime = -1.0;
    state = RhythmInfo {};
}
} // namespace eidolon
