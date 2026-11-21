#pragma once
// EIDOLON — MIDI capture (live buffer / fixed / triggered / phrase / loop) and internal replay.
#include "../Core/MidiEvent.h"
#include <deque>
#include <string>
#include <vector>

namespace eidolon
{
enum class CaptureMode : int { LiveBuffer, FixedDuration, Triggered, Phrase, Loop };
enum class CaptureState : int { Idle, Buffering, Armed, Recording, Done };

struct CaptureTake
{
    std::vector<MidiEvent> events;   // times relative to the take start (seconds)
    double length = 0.0;
    double bpm = 120.0;
    int noteCount = 0;
    std::string label;
    bool empty() const { return events.empty(); }
};

class CaptureEngine
{
public:
    static constexpr size_t kMaxEvents = 200000;

    void setMode (CaptureMode m);
    void setDuration (double seconds) { duration = seconds < 1.0 ? 1.0 : seconds; }
    void setPhraseGap (double s) { phraseGap = s; }
    CaptureMode mode() const { return captureMode; }
    CaptureState state() const { return captureState; }

    /** Feed every received (non-replayed) event. `activeNotes` = notes sounding after this event. */
    void onEvent (const MidiEvent& e, int activeNotes);
    void update (double now, int activeNotes, double bpm);

    void start (double now);   // Fixed: start now. Triggered / Phrase: arm. Loop / Live: (re)start buffering.
    void stop (double now);    // finishes the take (Live buffer: snapshot of the last `duration` seconds)
    void clearTake() { take = CaptureTake {}; }

    const CaptureTake& lastTake() const { return take; }
    double recordingElapsed (double now) const { return captureState == CaptureState::Recording ? now - recStart : 0.0; }
    size_t bufferedEvents() const { return buffer.size(); }
    double bufferedSeconds (double now) const { return buffer.empty() ? 0.0 : now - buffer.front().time; }
    std::string statusText (double now) const;
    int takesCompleted() const { return completed; }

    // ---- internal replay (feeds the visualization as if live) ----
    void startReplay (double now, bool loop);
    void stopReplay() { replaying = false; }
    bool isReplaying() const { return replaying; }
    float replayProgress (double now) const;
    /** Appends due replay events (times rebased to `now`) to `out`. */
    void pollReplay (double now, std::vector<MidiEvent>& out);

private:
    void finishTake (double endTime, double startTime);

    CaptureMode captureMode = CaptureMode::LiveBuffer;
    CaptureState captureState = CaptureState::Buffering;
    double duration = 30.0, phraseGap = 1.5;
    std::deque<MidiEvent> buffer;    // engine thread only; bounded by kMaxEvents
    double recStart = 0.0, lastActivity = 0.0, currentBpm = 120.0;
    CaptureTake take;
    int completed = 0;

    bool replaying = false, replayLoop = false;
    double replayStart = 0.0;
    size_t replayIndex = 0;
};
} // namespace eidolon
