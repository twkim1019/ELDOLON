// EIDOLON — core test suite (JUCE-free): MIDI engine, musical intelligence, mapping, capture / SMF export,
// Visual DNA + mutation. Usage: eidolon_core_tests [report.md]
#include "Capture/CaptureEngine.h"
#include "Capture/MidiFileWriter.h"
#include "Core/MathUtil.h"
#include "Core/Params.h"
#include "Core/SpscRing.h"
#include "DNA/FactoryPresets.h"
#include "DNA/MutationEngine.h"
#include "Intelligence/MusicalAnalyzer.h"
#include "Mapping/MappingEngine.h"
#include "MIDI/NoteTracker.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace eidolon;

static int passes = 0, fails = 0;
static std::string report;

static void check (bool ok, const std::string& name, const std::string& detail)
{
    (ok ? passes : fails)++;
    std::printf ("[%s] %-58s %s\n", ok ? "PASS" : "FAIL", name.c_str(), detail.c_str());
    report += "| " + std::string (ok ? "PASS" : "FAIL") + " | " + name + " | " + detail + " |\n";
}

template <typename... A> static std::string fmt (const char* f, A... a) { char b[512]; std::snprintf (b, sizeof (b), f, a...); return b; }

static MidiEvent on (double t, int note, int vel = 100, int ch = 0)  { MidiEvent e; e.time = t; e.type = EvType::NoteOn; e.data1 = (uint8_t) note; e.data2 = (uint8_t) vel; e.channel = (uint8_t) ch; return e; }
static MidiEvent off (double t, int note, int ch = 0)                { MidiEvent e; e.time = t; e.type = EvType::NoteOff; e.data1 = (uint8_t) note; e.channel = (uint8_t) ch; return e; }
static MidiEvent cc (double t, int n, int v, int ch = 0)             { MidiEvent e; e.time = t; e.type = EvType::Controller; e.data1 = (uint8_t) n; e.data2 = (uint8_t) v; e.channel = (uint8_t) ch; return e; }
static MidiEvent bend (double t, int v, int ch = 0)                  { MidiEvent e; e.time = t; e.type = EvType::PitchBend; e.value = (int16_t) v; e.channel = (uint8_t) ch; return e; }

static ChordInfo chordOf (std::initializer_list<int> notes, int minNotes = 3)
{
    ChordDetector d;
    ChordConfig c; c.minNotes = minNotes; d.setConfig (c);
    uint16_t mask = 0; std::array<float, 12> w {}; int lowest = 999;
    for (int n : notes) { mask = (uint16_t) (mask | (1u << (n % 12))); w[(size_t) (n % 12)] += 1.4f; lowest = std::min (lowest, n); }
    w[(size_t) (lowest % 12)] += 0.6f;
    return d.analyze (mask, w, lowest % 12);
}

// ------------------------------------------------------------------------------------------------- MIDI
static void midiTests()
{
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0.0, 60, 90), out);
        const bool started = out.size() == 1 && out[0].kind == NoteChange::Start && t.activeCount() == 1;
        t.process (off (0.5, 60), out);
        check (started && out.size() == 2 && out[1].kind == NoteChange::Release && t.activeCount() == 0 && out[1].id == out[0].id,
               "Note On / Note Off lifecycle", fmt ("%zu changes, active after off = %d", out.size(), t.activeCount()));
        check (out[0].velocity == 90 && out[1].onTime == 0.0 && out[1].time == 0.5, "Velocity and timestamps preserved",
               fmt ("vel %d, on %.2f, off %.2f", out[0].velocity, out[1].onTime, out[1].time));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0.0, 64, 80), out); t.process (on (0.1, 64, 0), out);
        check (out.size() == 2 && out[1].kind == NoteChange::Release && t.activeCount() == 0, "Note On velocity 0 = Note Off", fmt ("%zu changes", out.size()));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (cc (0.0, 64, 127), out); t.process (on (0.1, 60), out); t.process (off (0.2, 60), out);
        const bool held = t.activeCount() == 1 && t.keyDownCount() == 0 && out.back().kind == NoteChange::KeyUp;
        t.process (cc (0.5, 64, 0), out);
        check (held && t.activeCount() == 0 && out.back().kind == NoteChange::Release && out.back().time == 0.5,
               "Sustain pedal holds released keys", fmt ("held=%d, released at %.1f s", (int) held, out.back().time));
        t.setSustainAware (false);
        t.process (cc (1.0, 64, 127), out); t.process (on (1.1, 62), out); t.process (off (1.2, 62), out);
        check (t.activeCount() == 0, "Sustain-aware can be disabled", fmt ("active %d", t.activeCount()));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (bend (0, -8192), out); const float lo = t.pitchBend (0);
        t.process (bend (0, 8191), out);  const float hi = t.pitchBend (0);
        t.process (bend (0, 0), out);     const float mid = t.pitchBend (0);
        check (lo == -1.0f && hi == 1.0f && mid == 0.0f, "Pitch bend tracking (-8192..8191 -> -1..1)", fmt ("%.3f / %.3f / %.3f", lo, mid, hi));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (cc (0, 1, 64, 3), out); t.process (on (0, 60, 100, 3), out); t.process (on (0, 64, 100, 3), out);
        const float v = t.cc (3, 1);
        t.process (cc (0.1, 123, 0, 3), out);
        check (std::abs (v - 64.0f / 127.0f) < 1e-6f && t.activeCount() == 0, "CC tracking + All Notes Off (CC 123)",
               fmt ("CC1 = %.3f, active after CC123 = %d", v, t.activeCount()));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0, 60), out);
        MidiEvent at; at.type = EvType::PolyAftertouch; at.data1 = 60; at.data2 = 77; t.process (at, out);
        MidiEvent cp; cp.type = EvType::ChannelPressure; cp.data2 = 127; t.process (cp, out);
        check (t.slot (0, 60).aftertouch == 77 && t.pressure (0) == 1.0f, "Poly aftertouch + channel pressure",
               fmt ("poly AT %d, pressure %.2f", t.slot (0, 60).aftertouch, t.pressure (0)));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0, 60, 100, 0), out); t.process (on (0, 60, 100, 1), out); t.process (off (0.1, 60, 0), out);
        check (t.activeCount() == 1 && t.slot (1, 60).state == NoteState::Active, "Multiple channels are independent",
               fmt ("active %d (ch2 still on)", t.activeCount()));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        for (int n : { 60, 64, 67, 71 }) t.process (on (0, n), out);
        check (t.activeCount() == 4 && t.pitchClassMask (true) == ((1 << 0) | (1 << 4) | (1 << 7) | (1 << 11)) && t.lowestActive() == 60 && t.highestActive() == 71,
               "Simultaneous notes (polyphony, range, pitch classes)", fmt ("active %d, low %d, high %d", t.activeCount(), t.lowestActive(), t.highestActive()));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0, 60), out); t.process (on (0.1, 60), out);
        check (out.size() == 3 && out[1].kind == NoteChange::Release && out[2].kind == NoteChange::Start && out[0].id != out[2].id && t.activeCount() == 1,
               "Repeated note re-trigger (release + new instance)", fmt ("%zu changes, ids %u -> %u", out.size(), out[0].id, out[2].id));
    }
    {
        NoteTracker t; std::vector<NoteChange> out;
        t.process (on (0, 60), out); t.process (on (5, 62), out);
        const int r = t.releaseStuck (12.0, 10.0, out);
        check (r == 1 && t.activeCount() == 1 && out.back().forced && out.back().pitch == 60, "Stuck-note recovery (timeout)",
               fmt ("released %d stuck note, %d still active", r, t.activeCount()));
        t.panic (13.0, out);
        check (t.activeCount() == 0, "Panic releases everything", fmt ("active %d", t.activeCount()));
    }
    {
        SpscRing<MidiEvent, 1024> ring;
        int accepted = 0;
        for (int i = 0; i < 1500; ++i) if (ring.push (on (i * 0.001, i % 128))) ++accepted;
        bool ordered = true; MidiEvent e; int popped = 0;
        while (ring.pop (e)) { ordered = ordered && std::abs (e.time - popped * 0.001) < 1e-12; ++popped; }
        check (accepted == 1024 && popped == 1024 && ordered, "Event queue overflow is bounded and detected",
               fmt ("%d of 1500 accepted (capacity 1024), %d dropped", accepted, 1500 - accepted));
        check (ordered, "Event ordering (FIFO through the lock-free queue)", fmt ("%d events in order", popped));
    }
}

// ------------------------------------------------------------------------------------------------- intelligence
static void intelligenceTests()
{
    struct Case { std::initializer_list<int> notes; const char* expect; };
    const Case cases[] = {
        { { 60, 64, 67 }, "C" }, { { 60, 63, 67 }, "Cm" }, { { 60, 63, 66 }, "Cdim" }, { { 60, 64, 68 }, "Caug" },
        { { 60, 62, 67 }, "Csus2" }, { { 60, 65, 67 }, "Csus4" }, { { 60, 64, 67, 69 }, "C6" }, { { 60, 63, 67, 69 }, "Cm6" },
        { { 60, 64, 67, 70 }, "C7" }, { { 60, 64, 67, 71 }, "Cmaj7" }, { { 60, 63, 67, 70 }, "Cm7" }, { { 60, 63, 67, 71 }, "CmMaj7" },
        { { 60, 63, 66, 69 }, "Cdim7" }, { { 60, 63, 66, 70 }, "Cm7b5" }, { { 48, 64, 70, 74 }, "C9" }, { { 48, 64, 71, 74 }, "Cmaj9" },
        { { 48, 63, 70, 74 }, "Cm9" }, { { 48, 58, 62, 65 }, "C11" }, { { 48, 64, 69, 70 }, "C13" }, { { 60, 64, 70 }, "C7" },
    };
    int ok = 0; std::string wrong;
    for (const auto& c : cases)
    {
        const auto r = chordOf (c.notes);
        if (r.valid && r.name == c.expect) ++ok; else wrong += std::string (" ") + c.expect + "->" + (r.valid ? r.name : "none");
    }
    check (ok == (int) (sizeof (cases) / sizeof (cases[0])), "Chord recognition (20 chord types, root position)",
           fmt ("%d / %d correct%s", ok, (int) (sizeof (cases) / sizeof (cases[0])), wrong.c_str()));

    const auto i1 = chordOf ({ 52, 60, 67 }), i2 = chordOf ({ 55, 60, 64 }), i3 = chordOf ({ 46, 60, 64, 67 });
    check (i1.name == "C/E" && i1.inversion == 1 && i2.name == "C/G" && i2.inversion == 2 && i3.name == "C7/A#" && i3.inversion == 3,
           "Inversions (1st / 2nd / 3rd) + bass note", i1.name + ", " + i2.name + ", " + i3.name);
    const auto a = chordOf ({ 45, 60, 64, 67 }), b = chordOf ({ 48, 64, 67, 69 });
    check (a.name == "Am7" && b.name == "C6", "Bass context disambiguates (Am7 vs C6)", a.name + " / " + b.name);
    const auto aminv = chordOf ({ 48, 57, 64 });
    check (aminv.name == "Am/C", "Root estimation in inversion (A C E over C)", aminv.name);

    const auto cluster = chordOf ({ 60, 61, 62, 63 }), single = chordOf ({ 60 }), two = chordOf ({ 60, 64 });
    check (! cluster.valid && ! single.valid && ! two.valid, "Non-chords are not forced into chord labels",
           fmt ("cluster %s, single %s, dyad (min 3) %s", cluster.valid ? cluster.name.c_str() : "none", single.valid ? "chord" : "none", two.valid ? "chord" : "none"));
    const auto dyad = chordOf ({ 60, 67 }, 2), third = chordOf ({ 60, 64 }, 2);
    check (dyad.name == "C5" && third.templateIndex == -3, "Dyads with min-notes 2 (power chord / interval)", dyad.name + ", " + third.name + " " + third.typeName);

    const auto clean = chordOf ({ 60, 64, 67 }), dirty = chordOf ({ 60, 64, 67, 66 });
    check (clean.confidence > dirty.confidence + 0.1f, "Chord confidence drops with foreign tones",
           fmt ("clean %.2f vs +F# %.2f (%s)", clean.confidence, dirty.confidence, dirty.valid ? dirty.name.c_str() : "rejected"));

    {   // temporal stabilization: a 40 ms passing chord must not appear, a held chord must
        ChordDetector d; ChordConfig c; c.holdTime = 0.09; d.setConfig (c);
        std::array<float, 12> w; w.fill (1.0f);
        const uint16_t cmaj = (1 << 0) | (1 << 4) | (1 << 7), fmaj = (1 << 5) | (1 << 9) | (1 << 0), gmaj = (1 << 7) | (1 << 11) | (1 << 2);
        int changes = 0; double t = 0;
        for (; t < 0.5; t += 0.01) { d.update (t, cmaj, w, 0, { 60, 64, 67 }); changes += d.changed(); }
        for (double e = t + 0.04; t < e; t += 0.01) { d.update (t, fmaj, w, 5, { 53, 57, 60 }); changes += d.changed(); }
        for (double e = t + 0.5; t < e; t += 0.01) { d.update (t, gmaj, w, 7, { 55, 59, 62 }); changes += d.changed(); }
        const auto& h = d.history();
        check (changes == 2 && h.size() == 2 && h[0].chord.name == "C" && h[1].chord.name == "G",
               "Chord smoothing suppresses flicker", fmt ("%d committed changes (C -> G, 40 ms F skipped)", changes));
        check (h.size() == 2 && h[1].rootMotion == -5 && h[1].voiceLeadingSemitones == 8, "Chord transition history + voice leading",
               fmt ("root motion %d, nearest-voice movement %d semitones", h.size() == 2 ? h[1].rootMotion : 0, h.size() == 2 ? h[1].voiceLeadingSemitones : 0));
    }

    auto scaleOf = [] (std::initializer_list<int> notes, int reps = 3)
    {
        ScaleDetector s;
        for (int r = 0; r < reps; ++r) for (int n : notes) s.addNote (n, n == *notes.begin() ? 1.5f : 0.8f);
        return s.estimate (false);
    };
    const auto cmaj = scaleOf ({ 60, 62, 64, 65, 67, 69, 71, 72, 67, 64, 60 });
    const auto ahm = scaleOf ({ 57, 59, 60, 62, 64, 65, 68, 69, 64, 57 });
    const auto pent = scaleOf ({ 60, 62, 64, 67, 69, 72, 69, 67, 64, 60 });
    const auto blues = scaleOf ({ 57, 60, 62, 63, 64, 67, 69, 63, 57 });
    check (cmaj.name == "C Major" && ahm.name == "A Harmonic Minor" && pent.name == "C Major Pentatonic",
           "Scale estimation (major / harmonic minor / pentatonic)", cmaj.name + ", " + ahm.name + ", " + pent.name);
    check (blues.valid && cmaj.confidence > 0.3f && cmaj.top[1].scaleIndex >= 0,
           "Scale confidence + alternative candidates", fmt ("C major conf %.2f; blues-ish line -> %s (conf %.2f)", cmaj.confidence, blues.name.c_str(), blues.confidence));

    {   // statistics
        MidiHistory h;
        for (int i = 0; i < 12; ++i) { h.noteStarted ((uint32_t) i + 1, 0, 60 + (i % 3) * 12, 64 + i, i * 0.1); h.noteEnded ((uint32_t) i + 1, i * 0.1 + 0.25); }
        MusicalStatistics st; st.compute (h, 1.5, 5.0);
        const auto& d = st.windowed();
        check (d.noteCount == 12 && d.pitchClass[0] == 12.0f && d.octave[5] == 4.0f && d.octave[6] == 4.0f && d.lowest == 60 && d.highest == 84,
               "Pitch / pitch-class / octave distribution", fmt ("%d notes, PC C = %.0f, oct5 %.0f, range %d-%d", d.noteCount, d.pitchClass[0], d.octave[5], d.lowest, d.highest));
        MusicalStatistics inst; inst.compute (h, 1.5, 0.0);
        check (inst.windowed().noteCount == 0, "Instant window = sounding notes only", fmt ("%d sounding", inst.windowed().noteCount));
    }

    {   // voice tracking: soprano and bass interleaved
        VoiceTracker v; v.setMode (VoiceMode::Automatic); v.setMaxVoices (4);
        int sopranoVoice = -1, bassVoice = -1, consistent = 0, total = 0; uint32_t id = 1;
        for (int i = 0; i < 16; ++i)
        {
            const double t = i * 0.25;
            const int s = v.noteOn (id, 0, 72 + (i % 4), t); v.noteOff (id++, t + 0.2);
            const int b = v.noteOn (id, 0, 40 + (i % 3), t + 0.01); v.noteOff (id++, t + 0.22);
            if (i == 0) { sopranoVoice = s; bassVoice = b; }
            consistent += (s == sopranoVoice) + (b == bassVoice); total += 2;
        }
        check (sopranoVoice != bassVoice && consistent == total, "Voice tracking separates interleaved lines",
               fmt ("%d / %d assignments consistent, ambiguity %.2f", consistent, total, v.ambiguity()));
        VoiceTracker cv; cv.setMode (VoiceMode::Channel); cv.setMaxVoices (4);
        check (cv.noteOn (1, 2, 60, 0) == 2 && cv.noteOn (2, 1, 60, 0) == 1, "Voice tracking by MIDI channel", "ch3 -> voice 2, ch2 -> voice 1");
    }

    {   // rhythm: 120 BPM quarter notes, then 8 onsets / s
        RhythmAnalyzer r; TransportInfo tr;
        for (int i = 0; i < 24; ++i) { r.onset (i * 0.5, 100); r.update (i * 0.5 + 0.01, tr); }
        const float bpm = r.info().bpm;
        RhythmAnalyzer d;
        for (int i = 0; i < 40; ++i) { d.onset (i * 0.125, 100); d.update (i * 0.125 + 0.001, tr); }
        for (int k = 0; k < 20; ++k) d.update (5.0, tr);
        check (std::abs (bpm - 120.0f) <= 2.0f && r.info().source == BeatSource::Estimated, "Tempo estimation from onsets (120 BPM)", fmt ("%.1f BPM (estimated)", bpm));
        check (std::abs (d.info().onsetRate - 8.0f) < 1.0f, "Onset rate / rhythmic density", fmt ("%.2f onsets/s, density %.2f", d.info().onsetRate, d.info().density));
        RhythmAnalyzer h; TransportInfo host; host.valid = host.playing = true; host.bpm = 96; host.ppq = 6.5; host.barStartPpq = 4.0; host.audioTime = 10.0;
        h.update (10.0, host);
        check (h.info().source == BeatSource::Host && std::abs (h.info().beatPhase - 0.5f) < 1e-4f && std::abs (h.info().barPhase - 0.625f) < 1e-4f,
               "Beat / bar phase from host transport", fmt ("beat phase %.3f, bar phase %.3f, %.0f BPM", h.info().beatPhase, h.info().barPhase, h.info().bpm));
    }

    {   // full analyzer: polyphony, density, chord through the whole pipeline
        MusicalAnalyzer an; NoteTracker t; std::vector<NoteChange> ch; ParamSet p; an.configure (p);
        TransportInfo tr;
        auto feed = [&] (const MidiEvent& e) { ch.clear(); t.process (e, ch); an.onEvent (e); for (auto& c : ch) an.onNoteChange (c, t); };
        for (int n : { 48, 55, 64, 67, 72 }) feed (on (1.0, n, 100));
        for (double now = 1.0; now < 1.4; now += 0.016) an.update (now, 0.016, t, tr);
        const auto& s = an.snapshot();
        check (s.polyphony == 5 && s.chord.valid && s.chord.name == "C" && s.highest == 72 && s.lowest == 48,
               "Analyzer pipeline: polyphony + chord + range", fmt ("poly %d, chord %s, range %d-%d", s.polyphony, s.chord.name.c_str(), s.lowest, s.highest));
    }
}

// ------------------------------------------------------------------------------------------------- mapping
static void mappingTests()
{
    Mapping m;
    bool mono = true; float prev = -1;
    for (int c = 0; c < (int) MapCurve::Count; ++c)
    {
        m.curve = (MapCurve) c; prev = -1;
        for (int i = 0; i <= 100; ++i) { const float y = applyCurve (m, i / 100.0f); mono = mono && y >= prev - 1e-6f; prev = y; }
        mono = mono && std::abs (applyCurve (m, 0.0f)) < 1e-6f && std::abs (applyCurve (m, 1.0f) - 1.0f) < 1e-6f;
    }
    check (mono, "Mapping curves monotonic with fixed endpoints", "linear / exp / log / S / custom");

    ParamSet base; MappingInputs in;
    in.cc[0][1] = 1.0f; in.lastVelocityAny = 0.5f;
    std::vector<Mapping> maps (3);
    maps[0].source = MapSource::Velocity; maps[0].target = (int) P::PianoGlow; maps[0].outMin = 0; maps[0].outMax = 1; maps[0].smoothing = 0; maps[0].blend = MapBlend::Replace;
    maps[1].source = MapSource::CC; maps[1].ccNumber = 1; maps[1].target = (int) P::PianoGlow; maps[1].outMin = 0; maps[1].outMax = 0.2f; maps[1].smoothing = 0; maps[1].blend = MapBlend::Add;
    maps[2].source = MapSource::CC; maps[2].ccNumber = 1; maps[2].target = (int) P::PianoGlow; maps[2].outMin = 0; maps[2].outMax = 0.6f; maps[2].smoothing = 0; maps[2].blend = MapBlend::Min;
    ParamSet out; MappingEngine::process (maps, in, 0.016f, base, out);
    check (std::abs (out.get (P::PianoGlow) - 0.6f) < 1e-4f, "Arbitration: Replace -> Add -> Min", fmt ("0.5 + 0.2 limited to 0.6 = %.3f", out.get (P::PianoGlow)));
    maps[2].enabled = false; MappingEngine::process (maps, in, 0.016f, base, out);
    check (std::abs (out.get (P::PianoGlow) - 0.7f) < 1e-4f, "Multiple mappings on one target (Replace + Add)", fmt ("%.3f", out.get (P::PianoGlow)));

    std::vector<Mapping> env (1);
    env[0].source = MapSource::NoteOn; env[0].response = MapResponse::Envelope; env[0].decay = 0.2f; env[0].smoothing = 0; env[0].target = (int) P::LookBloom;
    MappingInputs hit; hit.noteOns[0] = 1; hit.lastVelocity[0] = 1.0f;
    MappingEngine::process (env, hit, 0.016f, base, out); const float peak = out.get (P::LookBloom);
    MappingInputs quiet; for (int i = 0; i < 30; ++i) MappingEngine::process (env, quiet, 0.016f, base, out);
    check (peak > 0.99f && out.get (P::LookBloom) < 0.15f, "Envelope response (Note On impulse decays)", fmt ("peak %.2f -> %.3f after 0.48 s", peak, out.get (P::LookBloom)));

    std::vector<Mapping> tg (1);
    tg[0].source = MapSource::CC; tg[0].ccNumber = 64; tg[0].response = MapResponse::Toggle; tg[0].target = (int) P::PianoOpacity;
    MappingInputs press; press.cc[0][64] = 1.0f; MappingInputs rel;
    MappingEngine::process (tg, press, 0.016f, base, out); const float t1 = out.get (P::PianoOpacity);
    MappingEngine::process (tg, rel, 0.016f, base, out); MappingEngine::process (tg, press, 0.016f, base, out);
    check (t1 == 1.0f && out.get (P::PianoOpacity) == 0.0f, "Toggle response flips on each press", fmt ("%.0f -> %.0f", t1, out.get (P::PianoOpacity)));

    std::vector<Mapping> dz (1);
    dz[0].source = MapSource::CC; dz[0].ccNumber = 7; dz[0].deadZone = 0.2f; dz[0].invert = true; dz[0].smoothing = 0; dz[0].target = (int) P::PianoOpacity;
    MappingInputs low; low.cc[0][7] = 0.1f;
    MappingEngine::process (dz, low, 0.016f, base, out);
    check (out.get (P::PianoOpacity) == 1.0f, "Dead zone + inversion", fmt ("input 0.1 in 0.2 dead zone, inverted -> %.2f", out.get (P::PianoOpacity)));
}

// ------------------------------------------------------------------------------------------------- capture
static void captureTests()
{
    {
        CaptureEngine c; c.setMode (CaptureMode::FixedDuration); c.setDuration (5.0); c.start (0.0);
        for (int i = 0; i < 40; ++i) { c.onEvent (on (i * 0.2, 60 + i % 12), 1); c.onEvent (off (i * 0.2 + 0.1, 60 + i % 12), 0); c.update (i * 0.2 + 0.15, 0, 120); }
        check (c.state() == CaptureState::Done && c.lastTake().noteCount == 25 && c.lastTake().length <= 5.0001,
               "Fixed-duration capture stops itself", fmt ("%d notes in %.2f s take", c.lastTake().noteCount, c.lastTake().length));
    }
    {
        CaptureEngine c; c.setMode (CaptureMode::Triggered); c.setDuration (15.0); c.start (0.0);
        c.update (3.0, 0, 120);
        const bool armed = c.state() == CaptureState::Armed;
        c.onEvent (on (4.0, 60), 1); c.onEvent (off (4.5, 60), 0); c.update (4.6, 0, 120); c.stop (5.0);
        check (armed && c.lastTake().noteCount == 1 && c.lastTake().events.front().time == 0.0, "Triggered capture starts on first note",
               fmt ("take starts at first note, %zu events", c.lastTake().events.size()));
    }
    {
        CaptureEngine c; c.setMode (CaptureMode::Phrase); c.setDuration (60.0); c.setPhraseGap (1.0); c.start (0.0);
        for (int i = 0; i < 8; ++i) { c.onEvent (on (1.0 + i * 0.3, 60 + i), 1); c.onEvent (off (1.2 + i * 0.3, 60 + i), 0); }
        for (double t = 3.4; t < 6.0; t += 0.05) c.update (t, 0, 120);
        check (c.state() == CaptureState::Done && c.lastTake().noteCount == 8 && c.lastTake().length < 2.6, "Phrase capture ends after silence gap",
               fmt ("%d notes, %.2f s", c.lastTake().noteCount, c.lastTake().length));
    }
    {
        CaptureEngine c; c.setMode (CaptureMode::LiveBuffer); c.setDuration (5.0);
        for (int i = 0; i < 100; ++i) { c.onEvent (on (i * 0.1, 60), 1); c.onEvent (off (i * 0.1 + 0.05, 60), 0); c.update (i * 0.1 + 0.06, 0, 120); }
        c.stop (10.0);
        check (c.lastTake().length <= 5.0001 && c.lastTake().noteCount >= 49 && c.lastTake().noteCount <= 51, "Live buffer keeps only the last N seconds",
               fmt ("%d notes in last %.1f s", c.lastTake().noteCount, c.lastTake().length));
        std::vector<MidiEvent> replay; c.startReplay (100.0, false);
        for (double t = 100.0; t < 106.0; t += 0.02) c.pollReplay (t, replay);
        bool flagged = true; for (auto& e : replay) flagged = flagged && (e.flags & MidiEvent::kReplayed);
        check (replay.size() == c.lastTake().events.size() && flagged && ! c.isReplaying(), "Internal replay emits every captured event",
               fmt ("%zu / %zu events replayed", replay.size(), c.lastTake().events.size()));
    }
    {
        std::vector<MidiEvent> ev = { on (0.0, 60, 100), cc (0.25, 1, 64), bend (0.3, 4096), off (0.5, 60), on (0.5, 64, 90), on (1.0, 67, 80) };
        SmfOptions o; o.bpm = 120;
        const auto bytes = writeStandardMidiFile (ev, 2.0, o);
        const auto s = summariseStandardMidiFile (bytes);
        check (s.ok && s.format == 0 && s.ppq == 960 && s.noteOns == 3 && s.noteOffs == 3 && s.controllers == 1 && s.pitchBends == 1 && s.lastTick == 3840,
               "Standard MIDI File export (parse-back)", fmt ("%zu bytes: %d on / %d off (2 hanging closed), %d CC, %d bend, end tick %u",
                                                             bytes.size(), s.noteOns, s.noteOffs, s.controllers, s.pitchBends, s.lastTick));
    }
}

// ------------------------------------------------------------------------------------------------- DNA
static void dnaTests()
{
    std::set<std::string> keys; bool rangesOk = true, choicesOk = true;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramInfo (i);
        keys.insert (p.key);
        rangesOk = rangesOk && p.defV >= p.minV && p.defV <= p.maxV && p.sanitise (p.defV) == p.defV;
        if (p.kind == PK::Choice) choicesOk = choicesOk && p.numChoices() == (int) p.maxV + 1 && p.minV == 0;
    }
    check ((int) keys.size() == kNumParams && rangesOk && choicesOk, "Parameter table (unique keys, valid defaults, choice counts)",
           fmt ("%d parameters", kNumParams));

    const auto& presets = factoryPresets();
    bool valid = true; std::set<std::string> names;
    for (const auto& d : presets)
    {
        names.insert (d.name);
        for (int i = 0; i < kNumParams; ++i) valid = valid && paramInfo (i).sanitise (d.params.v[(size_t) i]) == d.params.v[(size_t) i];
        for (const auto& m : d.mappings) valid = valid && paramInfo (m.target).isModulatable();
    }
    check (presets.size() >= 20 && names.size() == presets.size() && valid, "Factory Visual DNA presets valid", fmt ("%zu presets", presets.size()));

    MutationSettings s; s.intensity = 0.6f; s.seed = 4242;
    const auto a = MutationEngine::mutate (presets[0], s), b = MutationEngine::mutate (presets[0], s);
    s.seed = 4243; const auto c = MutationEngine::mutate (presets[0], s);
    int diff = 0; for (int i = 0; i < kNumParams; ++i) diff += a.params.v[(size_t) i] != presets[0].params.v[(size_t) i];
    check (a.params == b.params && ! (a.params == c.params) && diff > 10, "Mutation is reproducible (same seed = same DNA)",
           fmt ("%d parameters changed; seed 4243 differs", diff));
    MutationSettings keep = s; keep.preserve[(size_t) MG::Color] = true; keep.preserve[(size_t) MG::Camera] = true;
    const auto k = MutationEngine::mutate (presets[0], keep);
    bool kept = true;
    for (int i = 0; i < kNumParams; ++i)
        if (paramInfo (i).mutation == MG::Color || paramInfo (i).mutation == MG::Camera) kept = kept && k.params.v[(size_t) i] == presets[0].params.v[(size_t) i];
    check (kept, "Mutation preserves selected groups", "Color + Camera untouched");
    const auto vars = MutationEngine::variations (presets[0], s, 6);
    std::set<float> distinct; for (auto& v : vars) distinct.insert (v.params.get (P::PianoSize));
    check (vars.size() == 6 && distinct.size() >= 5, "Variation count + distinct variations", fmt ("%zu variations, %zu distinct sizes", vars.size(), distinct.size()));

    DNAHistory h; h.reset (presets[0]); h.push (presets[1]); h.push (presets[2]);
    const auto* u1 = h.undo(); const auto* u2 = h.undo(); const auto* r1 = h.redo();
    check (u1 && u1->name == presets[1].name && u2 && u2->name == presets[0].name && r1 && r1->name == presets[1].name && ! h.canUndo() == false,
           "Undo / redo history", "push 3, undo 2, redo 1");
}

// ------------------------------------------------------------------------------------------------- performance
static void performanceTests()
{
    MusicalAnalyzer an; NoteTracker t; std::vector<NoteChange> ch; ch.reserve (256); ParamSet p; an.configure (p);
    TransportInfo tr; Rng rng (7);
    const auto t0 = std::chrono::steady_clock::now();
    int events = 0; double now = 0;
    for (int frame = 0; frame < 3000; ++frame)   // 50 s at 60 fps, ~2000 events per second
    {
        for (int k = 0; k < 34; ++k)
        {
            const int n = 30 + rng.nextInt (70);
            const MidiEvent e = (rng.nextFloat() < 0.5f) ? on (now, n, 1 + rng.nextInt (126), rng.nextInt (16)) : off (now, n, rng.nextInt (16));
            ch.clear(); t.process (e, ch); an.onEvent (e); for (auto& c : ch) an.onNoteChange (c, t); ++events;
        }
        an.update (now, 1.0 / 60.0, t, tr);
        now += 1.0 / 60.0;
    }
    const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
    check (ms < 3000.0 * 16.6 * 0.25, "High MIDI density: analysis cost per frame", fmt ("%d events, %.3f ms per 60 fps frame (budget 16.6 ms)", events, ms / 3000.0));
    check (an.history().events.size() <= an.history().events.capacity() && an.history().notes.size() <= an.history().notes.capacity(),
           "History memory stays bounded", fmt ("%zu events / %zu notes retained", an.history().events.size(), an.history().notes.size()));
}

int main (int argc, char** argv)
{
    midiTests();
    intelligenceTests();
    mappingTests();
    captureTests();
    dnaTests();
    performanceTests();
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    if (argc > 1)
    {
        std::ofstream f (argv[1]);
        f << "| Result | Test | Measurement |\n|---|---|---|\n" << report << "\n**" << passes << " passed, " << fails << " failed**\n";
    }
    return fails == 0 ? 0 : 1;
}
