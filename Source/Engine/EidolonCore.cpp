#include "EidolonCore.h"
#include "../Capture/MidiFileWriter.h"
#include "../DNA/DNASerializer.h"
#include "../DNA/FactoryPresets.h"

namespace eidolon
{
EidolonCore::EidolonCore (AudioThreadShared& s) : shared (s)
{
    batch.reserve (32768);
    changes.reserve (4096);
    startMs = juce::Time::getMillisecondCounterHiRes();
    loadDNA (defaultDNA(), false);
    history.reset (currentDNA());
    configureCapture();
    rebuildPersisted();
    timerHz = 30;
    startTimerHz (timerHz);
}

EidolonCore::~EidolonCore() { stopTimer(); }

double EidolonCore::uiClock() const { return (juce::Time::getMillisecondCounterHiRes() - startMs) * 0.001; }

// ------------------------------------------------------------------------------------------------- DNA state
void EidolonCore::setParam (P p, float value)
{
    const float before = base.get (p);
    base.set (p, value);
    if (base.get (p) != before) markDirty();
    if (p == P::Mode) applyModeLayers (base, base.geti (P::Mode));
    if (paramInfo (p).group == PG::Capture) configureCapture();
}

void EidolonCore::setMode (int mode)
{
    if (mode >= 0 && mode <= 6) applyModeLayers (base, mode);
    else base.set (P::Mode, 7.0f);
    markDirty();
    commitUndo();
}

VisualDNA EidolonCore::currentDNA() const
{
    VisualDNA d = meta;
    d.params = base;
    for (auto& m : d.mappings) m.resetRuntime();
    return d;
}

void EidolonCore::loadDNA (const VisualDNA& d, bool undoable)
{
    meta = d;
    d.applyTo (base);
    for (auto& m : meta.mappings) m.resetRuntime();
    vis.scenes().requestScene (base.geti (P::SceneCurrent));
    markDirty();
    if (undoable) commitUndo();
}

void EidolonCore::commitUndo()
{
    history.push (currentDNA());
    markDirty();
}

void EidolonCore::undo()
{
    if (const auto* d = history.undo()) { meta = *d; d->applyTo (base); markDirty(); }
}

void EidolonCore::redo()
{
    if (const auto* d = history.redo()) { meta = *d; d->applyTo (base); markDirty(); }
}

void EidolonCore::loadFactoryProgram (int index)
{
    const auto& fp = factoryPresets();
    if (index < 0 || index >= (int) fp.size()) return;
    program = index;
    loadDNA (fp[(size_t) index], true);
    presets.markRecent (juce::String (fp[(size_t) index].name));
    rebuildPersisted();
}

void EidolonCore::panic()
{
    changes.clear();
    notes.panic (engineNow(), changes);
    for (auto& c : changes) analysis.onNoteChange (c, notes);
    vis.update (engineNow(), 0.0f, eff, analysis.snapshot(), notes, analysis.statistics(), analysis.statsWindowSeconds(), changes);
    changes.clear();
}

void EidolonCore::resetAnalysis()
{
    analysis.reset();
    vis.clear();
}

// ------------------------------------------------------------------------------------------------- capture
void EidolonCore::configureCapture()
{
    static const double durations[] = { 5.0, 15.0, 30.0, 60.0 };
    const int d = base.geti (P::CapDuration);
    captureEngine.setMode ((CaptureMode) base.geti (P::CapMode));
    captureEngine.setDuration (d < 4 ? durations[d] : base.get (P::CapCustom));
    captureEngine.setPhraseGap (base.get (P::CapPhraseGap));
}

bool EidolonCore::exportTake (const juce::File& file, juce::String& error) const
{
    const auto& take = captureEngine.lastTake();
    if (take.empty()) { error = "No capture yet: record a take first."; return false; }
    SmfOptions o;
    o.bpm = shared.transportValid.load() ? shared.bpm.load() : (analysis.snapshot().rhythm.bpm > 0 ? analysis.snapshot().rhythm.bpm : 120.0);
    o.numerator = shared.numerator.load();
    o.denominator = shared.denominator.load();
    const auto bytes = writeStandardMidiFile (take.events, take.length, o);
    if (! file.replaceWithData (bytes.data(), bytes.size())) { error = "Cannot write " + file.getFullPathName(); return false; }
    return true;
}

juce::File EidolonCore::writeTakeToTempFile() const
{
    const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("EIDOLON_capture_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".mid");
    juce::String err;
    return exportTake (f, err) ? f : juce::File();
}

// ------------------------------------------------------------------------------------------------- frame loop
void EidolonCore::setEditorAttached (bool attached)
{
    editorAttached = attached;
    const int hz = attached ? juce::jlimit (15, 120, base.geti (P::RenderFps)) : 30;
    if (hz != timerHz) { timerHz = hz; startTimerHz (hz); }
}

void EidolonCore::timerCallback()
{
    tick();
    if (onFrame) onFrame();
}

void EidolonCore::buildMappingInputs (const std::vector<NoteChange>& ch, MappingInputs& in) const
{
    const auto& a = analysis.snapshot();
    for (const auto& c : ch)
    {
        if (c.kind == NoteChange::Start)
        {
            ++in.noteOns[c.channel];
            in.lastVelocity[c.channel] = (float) c.velocity / 127.0f;
            in.lastPitch[c.channel] = (float) c.pitch / 127.0f;
        }
        else if (c.kind == NoteChange::Release) ++in.noteOffs[c.channel];
    }
    in.lastVelocityAny = a.recentVelocity / 127.0f;
    in.lastChannel = a.lastChannel >= 0 ? (float) a.lastChannel / 15.0f : 0.0f;
    double durSum = 0; int durN = 0; float highest = -1;
    const double now = engineNow();
    notes.forEachActive ([&] (int ch, int n, const NoteSlot& s)
    {
        durSum += now - s.onTime; ++durN;
        if ((float) n > highest) highest = (float) n;
        in.polyAT[(size_t) ch] = std::max (in.polyAT[(size_t) ch], (float) s.aftertouch / 127.0f);
        if (in.lastPitch[(size_t) ch] <= 0.0f) in.lastPitch[(size_t) ch] = (float) n / 127.0f;
    });
    in.lastPitchAny = highest >= 0 ? highest / 127.0f : (a.contourLine[(size_t) ((a.contourHead + 63) % 64)] / 127.0f);
    in.averageDuration = durN > 0 ? (float) (durSum / durN) : 0.0f;
    for (int c = 0; c < 16; ++c)
    {
        for (int n = 0; n < 128; ++n) in.cc[(size_t) c][(size_t) n] = notes.cc (c, n);
        in.bend[(size_t) c] = notes.pitchBend (c);
        in.pressure[(size_t) c] = notes.pressure (c);
        in.sustain[(size_t) c] = notes.sustainOn (c);
    }
    in.beatPhase = a.rhythm.beatPhase;
    in.barPhase = a.rhythm.barPhase;
    in.beatTick = a.rhythm.beatTick;
    in.barTick = a.rhythm.barTick;
    in.chordChanged = a.chordChanged;
    in.polyphony = a.polyphony;
    in.density = a.noteDensity;
}

void EidolonCore::tick()
{
    const double ui = uiClock();
    const float dt = (float) juce::jlimit (0.0, 0.1, ui - lastTickUi);
    lastTickUi = ui;

    if (restorePending.exchange (false))
    {
        juce::String json;
        { const juce::ScopedLock sl (stateLock); json = pendingRestore; }
        applyRestore (json);
    }
    const int prog = pendingProgram.exchange (-1);
    if (prog >= 0) loadFactoryProgram (prog);

    // host automation macros
    const int hs = shared.hostScene.load();
    if (hs >= 0 && hs != lastHostScene) { if (lastHostScene >= 0) vis.scenes().requestScene (hs); lastHostScene = hs; }
    const bool hn = shared.hostNextScene.load();
    if (hn && ! lastHostNext) vis.scenes().next();
    lastHostNext = hn;

    // ---- drain the audio thread queue (bounded per tick)
    batch.clear();
    MidiEvent e;
    double maxT = -1.0;
    while (batch.size() < 30000 && shared.queue.pop (e)) { batch.push_back (e); maxT = std::max (maxT, e.time); }
    if (maxT >= 0.0)
    {
        const double cand = ui - maxT;
        if (! offsetInit || std::abs (cand - clockOffset) > 0.3) { clockOffset = cand; offsetInit = true; }
        else clockOffset += (cand - clockOffset) * 0.05;
    }
    const double now = ui - clockOffset;

    // internal replay of a captured take
    const size_t live = batch.size();
    captureEngine.pollReplay (now, batch);
    if (wasReplaying && ! captureEngine.isReplaying()) panic();
    wasReplaying = captureEngine.isReplaying();

    notes.setSustainAware (base.getb (P::MidiSustain));
    analysis.configure (base);
    const int filter = base.geti (P::MidiChannel);
    changes.clear();
    for (size_t i = 0; i < batch.size(); ++i)
    {
        auto& ev = batch[i];
        if (i < live)
        {
            ++eventsSeen;
            lastMidiUi = ui;
            captureEngine.onEvent (ev, notes.activeCount());
        }
        const bool channelMsg = ev.type <= EvType::ProgramChange;
        if (filter > 0 && channelMsg && ev.channel != filter - 1) continue;
        if (channelMsg) lastChan = ev.channel;
        analysis.onEvent (ev);
        notes.process (ev, changes);
    }
    notes.releaseStuck (now, base.get (P::MidiStuck), changes);
    for (auto& c : changes)
    {
        const int v = analysis.onNoteChange (c, notes);
        if (c.kind == NoteChange::Start) c.voice = v;
    }

    analysis.update (now, dt, notes, [&]
    {
        TransportInfo t;
        t.valid = shared.transportValid.load();
        t.playing = shared.playing.load();
        t.bpm = shared.bpm.load();
        t.ppq = shared.ppq.load();
        t.barStartPpq = shared.barStartPpq.load();
        t.numerator = shared.numerator.load();
        t.denominator = shared.denominator.load();
        t.audioTime = shared.transportTime.load();
        return t;
    }());
    const auto& a = analysis.snapshot();
    captureEngine.update (now, notes.activeCount(), shared.transportValid.load() ? shared.bpm.load() : (double) a.rhythm.bpm);

    // ---- mapping: base parameters + host macros -> effective parameters
    ParamSet modBase = base;
    modBase.set (P::MotionEnergy, base.get (P::MotionEnergy) * shared.hostIntensity.load());
    modBase.set (P::MotionSpeed, base.get (P::MotionSpeed) * shared.hostMotion.load());
    modBase.set (P::PianoRelease, base.get (P::PianoRelease) * shared.hostRelease.load());
    MappingInputs in;
    buildMappingInputs (changes, in);
    MappingEngine::process (meta.mappings, in, dt, modBase, eff);

    vis.update (now, dt, eff, a, notes, analysis.statistics(), analysis.statsWindowSeconds(), changes);

    shared.midiThru.store (base.getb (P::MidiThru));
    if (editorAttached)
    {
        const int hz = juce::jlimit (15, 120, base.geti (P::RenderFps));
        if (hz != timerHz) { timerHz = hz; startTimerHz (hz); }
    }
    if (dirty && ui - lastPersist > 0.1) rebuildPersisted();
}

// ------------------------------------------------------------------------------------------------- persistence
void EidolonCore::rebuildPersisted()
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "EIDOLON-State");
    root->setProperty ("stateVersion", 1);
    root->setProperty ("pluginVersion", "1.0.0");
    root->setProperty ("settings", paramsToVar (base, false));
    root->setProperty ("dna", dnaToVar (currentDNA()));
    root->setProperty ("program", program);
    auto* u = new juce::DynamicObject();
    u->setProperty ("width", ui.width);
    u->setProperty ("height", ui.height);
    u->setProperty ("tab", ui.tab);
    u->setProperty ("panelVisible", ui.panelVisible);
    u->setProperty ("focusMode", ui.focusMode);
    root->setProperty ("ui", juce::var (u));
    const auto json = juce::JSON::toString (juce::var (root), true);
    { const juce::ScopedLock sl (stateLock); persisted = json; }
    dirty = false;
    lastPersist = uiClock();
}

juce::String EidolonCore::getStateJson() const
{
    const juce::ScopedLock sl (stateLock);
    return persisted;
}

void EidolonCore::restoreStateJson (const juce::String& json)
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr && juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyRestore (json);
        return;
    }
    { const juce::ScopedLock sl (stateLock); pendingRestore = json; }
    restorePending.store (true);
}

void EidolonCore::applyRestore (const juce::String& json)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || v.getProperty ("format", "").toString() != "EIDOLON-State") return;
    VisualDNA d;
    juce::String err;
    if (dnaFromVar (v.getProperty ("dna", {}), d, err)) loadDNA (d, false);
    paramsFromVar (v.getProperty ("settings", {}), base, false);   // exact values incl. non-DNA settings
    program = (int) v.getProperty ("program", 0);
    const auto u = v.getProperty ("ui", {});
    ui.width = juce::jlimit (720, 4000, (int) u.getProperty ("width", ui.width));
    ui.height = juce::jlimit (460, 3000, (int) u.getProperty ("height", ui.height));
    ui.tab = juce::jlimit (0, 7, (int) u.getProperty ("tab", ui.tab));
    ui.panelVisible = (bool) u.getProperty ("panelVisible", true);
    ui.focusMode = (bool) u.getProperty ("focusMode", false);
    configureCapture();
    vis.scenes().requestScene (base.geti (P::SceneCurrent));
    history.reset (currentDNA());
    markDirty();
    rebuildPersisted();
}
} // namespace eidolon
