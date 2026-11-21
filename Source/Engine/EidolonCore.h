#pragma once
// EIDOLON — engine core (message thread). Drains the lock-free MIDI queue filled by the audio thread,
// runs note tracking, musical intelligence, capture, mapping and the visualization engine, owns the
// Visual DNA state, undo / redo, the preset library and state persistence.
#include "../Capture/CaptureEngine.h"
#include "../Core/MidiEvent.h"
#include "../Core/SpscRing.h"
#include "../DNA/MutationEngine.h"
#include "../DNA/PresetLibrary.h"
#include "../Intelligence/MusicalAnalyzer.h"
#include "../Visualization/VisualizationEngine.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

namespace eidolon
{
using MidiQueue = SpscRing<MidiEvent, 16384>;

/** Shared between audio thread (writer) and engine (reader). Plain atomics: no locks on the audio thread. */
struct AudioThreadShared
{
    MidiQueue queue;
    std::atomic<uint64_t> received { 0 }, dropped { 0 }, thinned { 0 };
    std::atomic<bool> transportValid { false }, playing { false }, midiThru { true };
    std::atomic<double> bpm { 120.0 }, ppq { 0.0 }, barStartPpq { 0.0 }, transportTime { 0.0 }, blockEndTime { 0.0 };
    std::atomic<int> numerator { 4 }, denominator { 4 };
    std::atomic<float> hostIntensity { 1.0f }, hostMotion { 1.0f }, hostRelease { 1.0f };
    std::atomic<int> hostScene { -1 };
    std::atomic<bool> hostNextScene { false };
};

struct UiConfig
{
    int width = 1280, height = 780;
    int tab = 0;
    bool panelVisible = true;
    bool focusMode = false;
};

class EidolonCore : private juce::Timer
{
public:
    explicit EidolonCore (AudioThreadShared& shared);
    ~EidolonCore() override;

    // ---- parameters / Visual DNA (message thread)
    const ParamSet& params() const { return base; }
    const ParamSet& effective() const { return eff; }
    void setParam (P p, float value);          // marks state dirty; call commitUndo() when the gesture ends
    void setMode (int mode);
    VisualDNA currentDNA() const;
    VisualDNA& dnaMeta() { return meta; }      // name / category / seed / mappings (params live in params())
    void loadDNA (const VisualDNA& d, bool undoable = true);
    void commitUndo();
    bool canUndo() const { return history.canUndo(); }
    bool canRedo() const { return history.canRedo(); }
    void undo();
    void redo();
    void markDirty() { dirty = true; ++revision; }
    uint64_t stateRevision() const { return revision; }

    PresetLibrary& library() { return presets; }
    juce::String currentPresetName() const { return juce::String (meta.name); }
    void loadFactoryProgram (int index);
    int currentProgram() const { return program; }
    void requestProgram (int index) { pendingProgram.store (index); }

    // ---- engine pieces
    const NoteTracker& tracker() const { return notes; }
    const MusicalAnalyzer& analyzer() const { return analysis; }
    CaptureEngine& capture() { return captureEngine; }
    VisualizationEngine& visuals() { return vis; }
    void panic();
    void resetAnalysis();

    // ---- capture helpers
    void configureCapture();
    bool exportTake (const juce::File& file, juce::String& error) const;
    juce::File writeTakeToTempFile() const;

    // ---- MIDI status (never faked: driven only by events actually received)
    bool midiEverReceived() const { return eventsSeen > 0; }
    bool midiActive() const { return eventsSeen > 0 && uiClock() - lastMidiUi < 0.6; }
    uint64_t eventsReceived() const { return eventsSeen; }
    uint64_t eventsDropped() const { return shared.dropped.load(); }
    uint64_t eventsThinned() const { return shared.thinned.load(); }
    int lastChannel() const { return lastChan; }
    double engineNow() const { return uiClock() - clockOffset; }
    double uiClock() const;
    bool transportPlaying() const { return shared.playing.load(); }

    // ---- frame scheduling
    void setEditorAttached (bool attached);
    std::function<void()> onFrame;   // called after every engine tick (canvas repaints)

    // ---- persistence (getStateJson may be called from any thread)
    juce::String getStateJson() const;
    void flushStateIfDirty() { if (dirty) rebuildPersisted(); }   // message thread only
    void restoreStateJson (const juce::String& json);
    UiConfig ui;

    // mutation candidates shown in the DNA panel
    std::vector<VisualDNA> variations;
    MutationSettings mutation;

private:
    void timerCallback() override;
    void tick();
    void applyRestore (const juce::String& json);
    void rebuildPersisted();
    void buildMappingInputs (const std::vector<NoteChange>& changes, MappingInputs& in) const;

    AudioThreadShared& shared;
    ParamSet base, eff;
    VisualDNA meta;
    DNAHistory history;
    PresetLibrary presets;
    int program = 0;
    std::atomic<int> pendingProgram { -1 };

    NoteTracker notes;
    MusicalAnalyzer analysis;
    CaptureEngine captureEngine;
    VisualizationEngine vis;

    std::vector<MidiEvent> batch;
    std::vector<NoteChange> changes;
    double startMs = 0, lastTickUi = 0, clockOffset = 0, lastMidiUi = -10;
    bool offsetInit = false, editorAttached = false, dirty = true, wasReplaying = false;
    int lastHostScene = -1, lastChan = -1;
    bool lastHostNext = false;
    uint64_t eventsSeen = 0, revision = 1;
    double lastPersist = 0;
    int timerHz = 0;

    juce::CriticalSection stateLock;
    juce::String persisted, pendingRestore;
    std::atomic<bool> restorePending { false };
};
} // namespace eidolon
