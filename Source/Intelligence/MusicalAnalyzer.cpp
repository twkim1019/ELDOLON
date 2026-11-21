#include "MusicalAnalyzer.h"
#include "../Core/MathUtil.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace eidolon
{
namespace
{
bool keyPrefersFlats (const ScaleInfo& s)
{
    if (! s.valid || s.confidence < 0.2f) return false;
    const auto& types = ScaleDetector::scales();
    int majorRoot = s.root;
    if (types[(size_t) s.scaleIndex].profile == 1) majorRoot = (s.root + 3) % 12;
    if (s.scaleIndex == 8) majorRoot = (s.root + 10) % 12;   // dorian
    switch (majorRoot) { case 5: case 10: case 3: case 8: case 1: case 6: return true; default: return false; }
}
} // namespace

MusicalAnalyzer::MusicalAnalyzer()
{
    pitchScratch.reserve (128);
    recentOnsets.fill (-1e9);
    polyHistory.fill ({ -1e9, 0 });
}

void MusicalAnalyzer::configure (const ParamSet& p)
{
    ChordConfig cc;
    cc.holdTime = p.get (P::AnaChordHold);
    cc.releaseHold = p.get (P::AnaChordRelease);
    cc.minNotes = p.geti (P::AnaChordMinNotes);
    cc.minConfidence = p.get (P::AnaChordMinConf);
    cc.customTemplate = (uint16_t) (p.geti (P::AnaCustomChord) & 0x0FFF);
    chords.setConfig (cc);
    useSustainForChords = p.getb (P::AnaChordSustain);
    scaleDet.setTimeConstant (p.get (P::AnaScaleWindow));
    scaleDet.setCustomMask ((uint16_t) (p.geti (P::AnaCustomScale) & 0x0FFF));
    voices.setMode ((VoiceMode) p.geti (P::FlowVoiceMode));
    voices.setMaxVoices (p.geti (P::FlowMaxVoices));
    voices.setContinuityGap (p.get (P::AnaVoiceGap));
    switch (p.geti (P::SpecWindow))
    {
        case 0:  statsWindow = 0.0; break;
        case 1:  statsWindow = 1.0; break;
        case 2:  statsWindow = 5.0; break;
        case 3:  statsWindow = 10.0; break;
        case 4:  statsWindow = 30.0; break;
        case 5:  statsWindow = std::numeric_limits<double>::infinity(); break;
        default: statsWindow = p.get (P::SpecCustom); break;
    }
}

void MusicalAnalyzer::onEvent (const MidiEvent& e)
{
    ++snap.totalEvents;
    hist.addEvent (e);
    const int ch = e.channel & 15;
    switch (e.type)
    {
        case EvType::Controller:
        {
            for (size_t i = snap.recentCC.size() - 1; i > 0; --i) snap.recentCC[i] = snap.recentCC[i - 1];
            snap.recentCC[0] = { ch, e.data1, e.data2, e.time };
            snap.channelActivity[(size_t) ch] = std::max (snap.channelActivity[(size_t) ch], 0.35f);
            snap.lastChannel = ch;
            break;
        }
        case EvType::PitchBend:
        case EvType::ChannelPressure:
        case EvType::PolyAftertouch:
            snap.channelActivity[(size_t) ch] = std::max (snap.channelActivity[(size_t) ch], 0.25f);
            snap.lastChannel = ch;
            break;
        case EvType::Clock: case EvType::Start: case EvType::Stop: case EvType::Continue: case EvType::SongPosition:
            rhythm.midiClock (e);
            break;
        default: break;
    }
}

int MusicalAnalyzer::onNoteChange (const NoteChange& c, const NoteTracker& tracker)
{
    int voice = -1;
    switch (c.kind)
    {
        case NoteChange::Start:
        {
            hist.noteStarted (c.id, c.channel, c.pitch, c.velocity, c.time);
            voice = voices.noteOn (c.id, c.channel, c.pitch, c.time);
            hist.setVoice (c.id, voice);
            rhythm.onset (c.time, c.velocity);
            stats.notePlayed (c.pitch, c.velocity);
            recentOnsets[onsetHead] = c.time;
            onsetHead = (onsetHead + 1) % recentOnsets.size();
            snap.recentVelocity = snap.recentVelocity * 0.7f + (float) c.velocity * 0.3f;
            snap.channelActivity[c.channel] = 1.0f;
            snap.lastChannel = c.channel;
            ++snap.totalNotes;

            // melody = the highest sounding note (top-voice heuristic)
            int highestOther = -1;
            tracker.forEachActive ([&] (int, int n, const NoteSlot& s) { if (s.id != c.id && n > highestOther) highestOther = n; });
            const bool isTop = c.pitch >= highestOther || (lastMelodyTime >= 0.0 && c.time - lastMelodyTime > 0.6);
            if (isTop)
            {
                if (lastMelodyPitch >= 0) snap.lastInterval = (int) c.pitch - lastMelodyPitch;
                lastMelodyPitch = c.pitch;
                lastMelodyTime = c.time;
                snap.contourLine[(size_t) snap.contourHead] = (float) c.pitch;
                snap.contourHead = (snap.contourHead + 1) % (int) snap.contourLine.size();
                snap.contour = clampv (snap.contour * 0.75f + (float) snap.lastInterval / 12.0f * 0.25f * 2.0f, -1.0f, 1.0f);
            }
            break;
        }
        case NoteChange::Release:
        {
            hist.noteEnded (c.id, c.time);
            voices.noteOff (c.id, c.time);
            const double dur = std::clamp (c.time - c.onTime, 0.08, 2.0);
            scaleDet.addNote (c.pitch, (float) c.velocity / 127.0f * (float) dur);
            break;
        }
        case NoteChange::KeyUp: break;
    }
    return voice;
}

void MusicalAnalyzer::update (double now, double dt, const NoteTracker& tracker, const TransportInfo& transport)
{
    snap.now = now;
    snap.activeNotes = tracker.activeCount();
    snap.keysDown = tracker.keyDownCount();
    snap.polyphony = snap.activeNotes;
    snap.channelNotes.fill (0);

    std::array<float, 12> weights {};
    pitchScratch.clear();
    float velSum = 0.0f, pitchSum = 0.0f, maxPolyAT = 0.0f;
    int lowestChord = -1, highest = -1, lowest = -1;
    tracker.forEachActive ([&] (int ch, int n, const NoteSlot& s)
    {
        ++snap.channelNotes[(size_t) ch];
        velSum += s.velocity;
        pitchSum += (float) n;
        if (highest < 0 || n > highest) highest = n;
        if (lowest < 0 || n < lowest) lowest = n;
        maxPolyAT = std::max (maxPolyAT, (float) s.aftertouch / 127.0f);
        scaleDet.addNote (n, (float) s.velocity / 127.0f * (float) dt * 0.5f);
        if (s.state == NoteState::Sustained && ! useSustainForChords) return;
        weights[(size_t) (n % 12)] += 0.4f + (float) s.velocity / 127.0f;
        pitchScratch.push_back (n);
        if (lowestChord < 0 || n < lowestChord) lowestChord = n;
    });
    snap.highest = highest;
    snap.lowest = lowest;
    snap.averageVelocity = snap.activeNotes > 0 ? velSum / (float) snap.activeNotes : 0.0f;
    snap.maxPolyAftertouch = maxPolyAT;
    if (lowestChord >= 0) weights[(size_t) (lowestChord % 12)] += 0.6f;   // bass salience

    uint16_t mask = 0;
    for (int n : pitchScratch) mask = (uint16_t) (mask | (1u << (n % 12)));

    scaleDet.decay (dt);
    snap.scale = scaleDet.estimate (false);
    const bool flats = keyPrefersFlats (snap.scale);
    if (flats) snap.scale = scaleDet.estimate (true);
    chords.setPreferFlats (flats);
    chords.update (now, mask, weights, lowestChord >= 0 ? lowestChord % 12 : -1, pitchScratch);
    snap.chord = chords.current();
    snap.chordChanged = chords.changed();
    snap.chordConfidenceRaw = chords.rawConfidence();
    if (snap.chordChanged || snap.chordHistory.size() != chords.history().size())
    {
        snap.chordHistory = chords.history();
        if (! snap.chordHistory.empty())
        {
            snap.lastVoiceLeading = snap.chordHistory.back().voiceLeadingSemitones;
            snap.lastRootMotion = snap.chordHistory.back().rootMotion;
        }
    }

    rhythm.update (now, transport);
    snap.rhythm = rhythm.info();

    int recent = 0;
    for (double t : recentOnsets) if (now - t <= 2.0) ++recent;
    snap.noteDensity += ((float) recent / 2.0f - snap.noteDensity) * smoothCoef ((float) dt, 0.35f);

    if (snap.activeNotes > 0)
    {
        const float mean = pitchSum / (float) snap.activeNotes;
        snap.registerCenter += (mean - snap.registerCenter) * smoothCoef ((float) dt, 0.6f);
        const float spread = (float) std::max (6, highest - lowest);
        snap.registerSpread += (spread - snap.registerSpread) * smoothCoef ((float) dt, 0.8f);
    }

    if (polyHistory[(polyHead + polyHistory.size() - 1) % polyHistory.size()].second != snap.polyphony)
    {
        polyHistory[polyHead] = { now, snap.polyphony };
        polyHead = (polyHead + 1) % polyHistory.size();
    }
    int peak = snap.polyphony;
    for (const auto& ph : polyHistory) if (now - ph.first <= 2.0) peak = std::max (peak, ph.second);
    snap.peakPolyphony = peak;

    for (int c = 0; c < 16; ++c)
    {
        snap.channelActivity[(size_t) c] *= std::exp (-(float) dt * 2.5f);
        snap.sustain[(size_t) c] = tracker.sustainOn (c);
        snap.pitchBend[(size_t) c] = tracker.pitchBend (c);
        snap.pressure[(size_t) c] = tracker.pressure (c);
    }
    if (now - lastMelodyTime > 1.5) snap.contour *= std::exp (-(float) dt * 0.8f);

    snap.voicesActive = voices.activeVoiceCount();
    snap.voiceAmbiguity = voices.ambiguity();

    if (lastStats < 0.0 || now - lastStats > 0.1)
    {
        stats.compute (hist, now, statsWindow);
        snap.meanDuration = stats.windowed().meanDuration;
        lastStats = now;
    }
    stats.sampleTimeline (now, snap.polyphony, snap.noteDensity);
}

void MusicalAnalyzer::reset()
{
    hist.clear();
    chords.reset();
    scaleDet.reset();
    voices.reset();
    rhythm.reset();
    stats.reset();
    snap = AnalysisSnapshot {};
    recentOnsets.fill (-1e9);
    polyHistory.fill ({ -1e9, 0 });
    lastMelodyPitch = -1;
    lastMelodyTime = -1.0;
}
} // namespace eidolon
