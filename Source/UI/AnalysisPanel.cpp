// EIDOLON — live musical analysis readout, custom chord / scale template editors, scene selector and the
// camera / performance / MIDI-input information headers.
#include "Panels.h"
#include "../Intelligence/ChordDetector.h"
#include "../Scenes/SceneManager.h"

namespace eidolon::ui
{
namespace
{
juce::String noteName (int n) { return n < 0 ? juce::String ("-") : juce::String (ChordDetector::pitchName (n % 12, false)) + juce::String (n / 12 - 1); }
const char* beatSourceName (BeatSource s)
{
    switch (s) { case BeatSource::Host: return "host"; case BeatSource::MidiClock: return "MIDI clock"; case BeatSource::Estimated: return "estimated"; default: return "none"; }
}
} // namespace

void AnalysisReadout::paint (juce::Graphics& g)
{
    const auto& a = core.analyzer().snapshot();
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (r, 5.0f);
    r = r.reduced (10.0f, 8.0f);
    float y = r.getY();
    auto row = [&] (const juce::String& k, const juce::String& v, juce::Colour vc = colours::text)
    {
        g.setFont (Fonts::get().regular (11.5f));
        g.setColour (colours::text3);
        g.drawText (k, juce::Rectangle<float> (r.getX(), y, r.getWidth() * 0.42f, 17.0f), juce::Justification::centredLeft, true);
        g.setFont (Fonts::get().mono (11.0f));
        g.setColour (vc);
        g.drawText (v, juce::Rectangle<float> (r.getX() + r.getWidth() * 0.42f, y, r.getWidth() * 0.58f, 17.0f), juce::Justification::centredLeft, true);
        y += 17.0f;
    };

    // active notes
    juce::StringArray names;
    core.tracker().forEachActive ([&] (int, int n, const NoteSlot&) { if (names.size() < 10) names.add (noteName (n)); });
    row ("Active notes", names.isEmpty() ? juce::String ("-") : names.joinIntoString (" "));
    row ("Count / polyphony", juce::String (a.activeNotes) + " sounding, " + juce::String (a.keysDown) + " keys, peak " + juce::String (a.peakPolyphony));
    row ("Highest / lowest", noteName (a.highest) + " / " + noteName (a.lowest));
    row ("Average velocity", juce::String (a.averageVelocity, 1) + "  (recent " + juce::String ((int) std::lround (a.recentVelocity)) + ")");
    row ("Note density", juce::String (a.noteDensity, 2) + " notes/s");
    row ("Rhythmic density", juce::String (a.rhythm.onsetRate, 2) + " onsets/s (" + juce::String ((int) (a.rhythm.density * 100)) + "%)");
    row ("Tempo / beat", a.rhythm.source == BeatSource::None ? juce::String ("-")
                        : juce::String (a.rhythm.bpm, 1) + " BPM, " + beatSourceName (a.rhythm.source) + ", beat " + juce::String (a.rhythm.beatInBar + 1) + "/" + juce::String (a.rhythm.beatsPerBar));
    row ("Mean duration", juce::String (a.meanDuration, 2) + " s");
    y += 6.0f;
    if (a.chord.valid)
    {
        juce::String inv = a.chord.inversion == 0 ? "root position" : a.chord.inversion == -1 ? "slash bass" : juce::String (a.chord.inversion) + (a.chord.inversion == 1 ? "st" : a.chord.inversion == 2 ? "nd" : "rd") + " inversion";
        row ("Chord", juce::String (a.chord.name), colours::accent);
        row ("Type / inversion", juce::String (a.chord.typeName) + ", " + inv);
        row ("Root / bass", juce::String (ChordDetector::pitchName (a.chord.root, false)) + " / " + (a.chord.bass >= 0 ? juce::String (ChordDetector::pitchName (a.chord.bass, false)) : juce::String ("-")));
        row ("Confidence", juce::String ((int) std::lround (a.chord.confidence * 100)) + "%  (raw " + juce::String ((int) std::lround (a.chordConfidenceRaw * 100)) + "%)");
    }
    else
    {
        row ("Chord", a.activeNotes > 0 ? "not a recognized chord" : "-", colours::text3);
        row ("Type / inversion", "-"); row ("Root / bass", "-");
        row ("Confidence", juce::String ((int) std::lround (a.chordConfidenceRaw * 100)) + "% (below threshold)");
    }
    juce::StringArray hist;
    for (size_t i = a.chordHistory.size() > 6 ? a.chordHistory.size() - 6 : 0; i < a.chordHistory.size(); ++i) hist.add (juce::String (a.chordHistory[i].chord.name));
    row ("Transitions", hist.isEmpty() ? juce::String ("-") : hist.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 "))));
    row ("Voice leading", juce::String (a.lastVoiceLeading) + " semitones moved, root " + (a.lastRootMotion >= 0 ? "+" : "") + juce::String (a.lastRootMotion));
    y += 6.0f;
    if (a.scale.valid)
    {
        row ("Scale (estimate)", juce::String (a.scale.name), colours::accent2);
        row ("Confidence", juce::String ((int) std::lround (a.scale.confidence * 100)) + "%");
        juce::StringArray alts;
        for (const auto& c : a.scale.top)
            if (c.scaleIndex >= 0) alts.add (juce::String (ChordDetector::pitchName (c.root, false)) + " " + ScaleDetector::scales()[(size_t) c.scaleIndex].name + " " + juce::String ((int) std::lround (c.fit * 100)) + "%");
        row ("Compatibility", alts.joinIntoString (", "));
    }
    else { row ("Scale (estimate)", "- (needs more notes)", colours::text3); row ("Confidence", "-"); row ("Compatibility", "-"); }
    row ("Melodic contour", a.contour > 0.15f ? "ascending" : a.contour < -0.15f ? "descending" : "level");
    row ("Voices", juce::String (a.voicesActive) + " active, ambiguity " + juce::String ((int) std::lround (a.voiceAmbiguity * 100)) + "% (heuristic)");
    juce::String sustain; for (int c = 0; c < 16; ++c) if (a.sustain[(size_t) c]) sustain << (sustain.isEmpty() ? "" : " ") << (c + 1);
    row ("Sustain", sustain.isEmpty() ? juce::String ("off") : "on (ch " + sustain + ")");
    float maxBend = 0; int bendCh = 0;
    for (int c = 0; c < 16; ++c) if (std::abs (a.pitchBend[(size_t) c]) > std::abs (maxBend)) { maxBend = a.pitchBend[(size_t) c]; bendCh = c; }
    row ("Pitch bend", maxBend == 0.0f ? juce::String ("centered") : juce::String (maxBend, 2) + " (ch " + juce::String (bendCh + 1) + ")");
    const auto& cc = a.recentCC[0];
    row ("Last CC", cc.channel < 0 ? juce::String ("-") : "CC" + juce::String (cc.number) + " = " + juce::String (cc.value) + " (ch " + juce::String (cc.channel + 1) + ")");
    row ("Events", juce::String ((juce::int64) core.eventsReceived()) + " received, " + juce::String ((juce::int64) core.eventsDropped()) + " dropped, "
                   + juce::String ((juce::int64) core.eventsThinned()) + " thinned");

    // channel activity bars
    y += 6.0f;
    g.setColour (colours::text3);
    g.setFont (Fonts::get().caps (9.5f));
    g.drawText ("CHANNEL ACTIVITY", juce::Rectangle<float> (r.getX(), y, r.getWidth(), 14.0f), juce::Justification::centredLeft);
    y += 16.0f;
    const float bw = r.getWidth() / 16.0f;
    for (int c = 0; c < 16; ++c)
    {
        const float v = a.channelActivity[(size_t) c];
        g.setColour (colours::bg3.brighter (0.1f));
        g.fillRect (r.getX() + (float) c * bw + 1, y, bw - 2, 16.0f);
        g.setColour (colours::accent.withAlpha (0.25f + 0.75f * v));
        g.fillRect (r.getX() + (float) c * bw + 1, y + 16.0f * (1.0f - v), bw - 2, 16.0f * v);
        g.setColour (colours::text3);
        g.setFont (Fonts::get().mono (8.5f));
        g.drawText (juce::String (c + 1), juce::Rectangle<float> (r.getX() + (float) c * bw, y + 17.0f, bw, 10.0f), juce::Justification::centred);
    }
}

void BitMaskEditor::paint (juce::Graphics& g)
{
    const int mask = core.params().geti (p);
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::text2);
    g.setFont (Fonts::get().regular (12.0f));
    g.drawText (label, r.removeFromTop (18.0f), juce::Justification::centredLeft);
    const float w = r.getWidth() / 12.0f;
    static const char* iv[] = { "R", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
    for (int i = 0; i < 12; ++i)
    {
        auto c = juce::Rectangle<float> (r.getX() + (float) i * w + 1.0f, r.getY() + 2.0f, w - 2.0f, r.getHeight() - 4.0f);
        const bool on = (mask >> i) & 1;
        g.setColour (on ? colours::accent.withAlpha (0.3f) : colours::bg3);
        g.fillRoundedRectangle (c, 3.0f);
        g.setColour (on ? colours::accent : colours::line2);
        g.drawRoundedRectangle (c, 3.0f, 1.0f);
        g.setColour (on ? juce::Colours::white : colours::text3);
        g.setFont (Fonts::get().medium (10.0f));
        g.drawText (iv[i], c, juce::Justification::centred);
    }
}

void BitMaskEditor::mouseDown (const juce::MouseEvent& e)
{
    const float w = (float) getWidth() / 12.0f;
    const int i = juce::jlimit (0, 11, (int) ((float) e.x / w));
    if (e.y < 18) return;
    int mask = core.params().geti (p) ^ (1 << i);
    core.setParam (p, (float) mask);
    core.commitUndo();
    repaint();
}

// ------------------------------------------------------------------------------------------------- scenes
SceneHeader::SceneHeader (EidolonCore& c) : core (c)
{
    addAndMakeVisible (nextButton);
    nextButton.onClick = [this] { core.visuals().scenes().next(); };
    startTimerHz (10);
}

juce::Rectangle<int> SceneHeader::cell (int i) const
{
    const int cw = (getWidth() - 4 * 6) / 5;
    return { (i % 5) * (cw + 6), (i / 5) * 46, cw, 40 };
}

void SceneHeader::resized() { nextButton.setBounds (0, 98, getWidth(), 26); }

void SceneHeader::paint (juce::Graphics& g)
{
    const auto& sm = core.visuals().scenes();
    for (int i = 0; i < kNumScenes; ++i)
    {
        const auto r = cell (i).toFloat();
        const bool cur = sm.current() == i;
        g.setColour (cur ? colours::accent.withAlpha (0.16f) : colours::bg3);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cur ? colours::accent : colours::line2);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        if (cur && sm.transitioning())
        {
            g.setColour (colours::accent.withAlpha (0.5f));
            g.fillRect (r.getX() + 3, r.getBottom() - 4, (r.getWidth() - 6) * sm.progress(), 2.0f);
        }
        g.setColour (cur ? juce::Colours::white : colours::text2);
        g.setFont (Fonts::get().medium (11.0f));
        g.drawText (sceneName (i), r, juce::Justification::centred, true);
    }
    g.setColour (colours::text3);
    g.setFont (Fonts::get().regular (11.0f));
    g.drawText (sm.transitioning() ? juce::String ("Transitioning from ") + sceneName (sm.previous()) + " to " + sceneName (sm.current())
                                   : juce::String ("Click a scene to transition"), juce::Rectangle<int> (0, 128, getWidth(), 18), juce::Justification::centredLeft);
}

void SceneHeader::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumScenes; ++i)
        if (cell (i).contains (e.getPosition()))
        {
            core.setParam (P::SceneCurrent, (float) i);
            core.commitUndo();
            repaint();
        }
}

// ------------------------------------------------------------------------------------------------- info headers
InfoHeader::InfoHeader (EidolonCore& c, int k) : core (c), kind (k)
{
    actionButton.setButtonText (kind == 0 ? "Reset camera" : kind == 1 ? "Reset analysis & visuals" : "Panic (all notes off)");
    actionButton.onClick = [this]
    {
        if (kind == 0) core.visuals().cameraController().reset();
        else if (kind == 1) core.resetAnalysis();
        else if (kind == 2) core.panic();
    };
    addAndMakeVisible (actionButton);
    startTimerHz (6);
}

void InfoHeader::resized() { actionButton.setBounds (getLocalBounds().removeFromBottom (26)); }

void InfoHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedBottom (32).toFloat();
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (r, 5.0f);
    r = r.reduced (10, 6);
    g.setFont (Fonts::get().regular (11.5f));
    juce::StringArray lines;
    if (kind == 0)
    {
        lines.add ("Drag: orbit    Right-drag / Shift-drag: pan");
        lines.add ("Wheel: zoom    Double-click: reset");
        lines.add ("Keys: R reset, F focus, N next scene, 1-7 modes");
    }
    else if (kind == 1)
    {
        const auto& s = core.visuals().stats();
        lines.add ("Renderer: " + (gpuStatus ? gpuStatus() : juce::String ("CPU")));
        lines.add ("FPS " + juce::String (s.fps, 1) + "   frame " + juce::String (s.renderMs, 1) + " ms   update " + juce::String (s.updateMs, 2) + " ms");
        lines.add ("Quality " + juce::String ((int) std::lround (s.quality * 100)) + "%   notes " + juce::String (s.notes) + "   particles " + juce::String (s.particles));
    }
    else
    {
        lines.add (core.midiActive() ? "MIDI: receiving" : core.midiEverReceived() ? "MIDI: idle (no events in the last 0.6 s)" : "MIDI: nothing received yet");
        lines.add ("Events: " + juce::String ((juce::int64) core.eventsReceived()) + "   dropped " + juce::String ((juce::int64) core.eventsDropped()));
        lines.add ("Host transport: " + juce::String (core.transportPlaying() ? "playing" : "stopped"));
    }
    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 0 ? colours::text : colours::text2);
        g.drawText (lines[i], r.withY (r.getY() + (float) i * 17.0f).withHeight (17.0f), juce::Justification::centredLeft, true);
    }
}
} // namespace eidolon::ui
