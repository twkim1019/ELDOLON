#include "CaptureEngine.h"
#include <algorithm>
#include <cstdio>

namespace eidolon
{
void CaptureEngine::setMode (CaptureMode m)
{
    if (m == captureMode) return;
    captureMode = m;
    buffer.clear();
    captureState = (m == CaptureMode::LiveBuffer || m == CaptureMode::Loop) ? CaptureState::Buffering : CaptureState::Idle;
}

void CaptureEngine::onEvent (const MidiEvent& e, int activeNotes)
{
    if (e.flags & MidiEvent::kReplayed) return;
    if (e.type == EvType::Clock) return;   // 24 ppq clock would flood the buffer; tempo is stored separately
    if (e.isNote() || activeNotes > 0) lastActivity = e.time;

    if (captureState == CaptureState::Armed && e.type == EvType::NoteOn && e.data2 > 0)
    {
        captureState = CaptureState::Recording;
        recStart = e.time;
        buffer.clear();
    }
    if (captureState == CaptureState::Recording || captureState == CaptureState::Buffering)
    {
        if (buffer.size() >= kMaxEvents) buffer.pop_front();
        buffer.push_back (e);
    }
}

void CaptureEngine::update (double now, int activeNotes, double bpm)
{
    currentBpm = bpm > 0 ? bpm : currentBpm;
    if (activeNotes > 0) lastActivity = now;
    switch (captureState)
    {
        case CaptureState::Buffering:
        {
            const double keep = captureMode == CaptureMode::Loop ? duration : duration;
            if (captureMode == CaptureMode::Loop)
            {
                if (buffer.empty()) recStart = now;
                if (now - recStart >= duration)
                {
                    finishTake (recStart + duration, recStart);
                    // carry over events past the loop boundary
                    const double boundary = recStart + duration;
                    while (! buffer.empty() && buffer.front().time < boundary) buffer.pop_front();
                    recStart = boundary;
                }
            }
            else
            {
                while (! buffer.empty() && now - buffer.front().time > keep) buffer.pop_front();
            }
            break;
        }
        case CaptureState::Recording:
        {
            const bool timeUp = now - recStart >= duration;
            const bool phraseEnd = captureMode == CaptureMode::Phrase && activeNotes == 0 && now - lastActivity >= phraseGap;
            if (timeUp || phraseEnd) stop (now);
            break;
        }
        case CaptureState::Idle:
        case CaptureState::Armed:
        case CaptureState::Done: break;
    }
}

void CaptureEngine::start (double now)
{
    buffer.clear();
    switch (captureMode)
    {
        case CaptureMode::FixedDuration: captureState = CaptureState::Recording; recStart = now; break;
        case CaptureMode::Triggered:
        case CaptureMode::Phrase:        captureState = CaptureState::Armed; break;
        case CaptureMode::LiveBuffer:
        case CaptureMode::Loop:          captureState = CaptureState::Buffering; recStart = now; break;
    }
}

void CaptureEngine::stop (double now)
{
    switch (captureState)
    {
        case CaptureState::Recording:
        {
            double end = now;
            if (captureMode == CaptureMode::Phrase) end = std::min (now, lastActivity + 0.25);
            finishTake (std::min (end, recStart + duration), recStart);
            buffer.clear();
            captureState = CaptureState::Done;
            break;
        }
        case CaptureState::Buffering:
        {
            const double startT = captureMode == CaptureMode::Loop ? recStart : std::max (buffer.empty() ? now : buffer.front().time, now - duration);
            finishTake (now, startT);
            if (captureMode == CaptureMode::Loop) { buffer.clear(); recStart = now; }
            break;   // live buffer keeps running
        }
        case CaptureState::Armed: captureState = CaptureState::Idle; break;
        case CaptureState::Idle:
        case CaptureState::Done: break;
    }
}

void CaptureEngine::finishTake (double endTime, double startTime)
{
    CaptureTake t;
    t.bpm = currentBpm;
    t.length = std::max (0.0, endTime - startTime);
    t.events.reserve (buffer.size());
    for (const auto& e : buffer)
    {
        if (e.time < startTime || e.time >= endTime) continue;   // [start, end)
        MidiEvent r = e;
        r.time -= startTime;
        if (r.type == EvType::NoteOn && r.data2 > 0) ++t.noteCount;
        t.events.push_back (r);
    }
    static const char* names[] = { "Live buffer", "Fixed", "Triggered", "Phrase", "Loop" };
    char label[96];
    std::snprintf (label, sizeof (label), "%s take - %.1f s, %d notes", names[(int) captureMode], t.length, t.noteCount);
    t.label = label;
    if (t.noteCount > 0 || ! t.events.empty() || captureMode != CaptureMode::Loop)
    {
        take = std::move (t);
        ++completed;
    }
}

std::string CaptureEngine::statusText (double now) const
{
    char s[96];
    switch (captureState)
    {
        case CaptureState::Idle:      return "Idle";
        case CaptureState::Armed:     return "Armed - waiting for first note";
        case CaptureState::Recording: std::snprintf (s, sizeof (s), "Recording %.1f / %.0f s", now - recStart, duration); return s;
        case CaptureState::Buffering:
            if (captureMode == CaptureMode::Loop) { std::snprintf (s, sizeof (s), "Looping %.1f / %.0f s", now - recStart, duration); return s; }
            std::snprintf (s, sizeof (s), "Buffering last %.0f s (%zu events)", duration, buffer.size()); return s;
        case CaptureState::Done:      return "Take ready";
    }
    return {};
}

void CaptureEngine::startReplay (double now, bool loop)
{
    if (take.empty()) return;
    replaying = true;
    replayLoop = loop;
    replayStart = now;
    replayIndex = 0;
}

float CaptureEngine::replayProgress (double now) const
{
    if (! replaying || take.length <= 0.0) return 0.0f;
    return (float) std::clamp ((now - replayStart) / take.length, 0.0, 1.0);
}

void CaptureEngine::pollReplay (double now, std::vector<MidiEvent>& out)
{
    if (! replaying) return;
    const double t = now - replayStart;
    while (replayIndex < take.events.size() && take.events[replayIndex].time <= t)
    {
        MidiEvent e = take.events[replayIndex++];
        e.time = replayStart + e.time;
        e.flags |= MidiEvent::kReplayed;
        out.push_back (e);
    }
    if (replayIndex >= take.events.size() && t >= take.length)
    {
        if (replayLoop && take.length > 0.05) { replayStart = now; replayIndex = 0; }
        else replaying = false;
    }
}
} // namespace eidolon
