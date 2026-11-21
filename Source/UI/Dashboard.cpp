// EIDOLON — musical dashboard overlay: chord, scale estimate, notes (with mini keyboard), rhythm (contour,
// beat), channels and controllers. Configurable visibility, position, size, style, update rate and smoothing.
#include "VisualizationCanvas.h"
#include "../Intelligence/ChordDetector.h"

namespace eidolon::ui
{
// ------------------------------------------------------------------------------------------------- dashboard
void drawDashboard (juce::Graphics& g, juce::Rectangle<float> canvas, EidolonCore& core, DashboardState& st)
{
    const auto& p = core.params();
    const auto& a = core.analyzer().snapshot();
    const double now = core.uiClock();
    const float k = 1.0f - p.get (P::DashSmooth) * 0.9f;
    if (st.lastUpdate < 0 || now - st.lastUpdate >= 1.0 / std::max (2, p.geti (P::DashRate)))
    {
        st.lastUpdate = now;
        st.velocity += (a.averageVelocity - st.velocity) * k;
        st.density += (a.noteDensity - st.density) * k;
        st.onset += (a.rhythm.onsetRate - st.onset) * k;
        st.polyphony += ((float) a.polyphony - st.polyphony) * k;
        st.chordConf += ((a.chord.valid ? a.chord.confidence : 0.0f) - st.chordConf) * k;
        st.scaleConf += ((a.scale.valid ? a.scale.confidence : 0.0f) - st.scaleConf) * k;
        st.chord = a.chord.valid ? juce::String (a.chord.name) : juce::String (a.activeNotes > 0 ? "?" : "-");
        st.chordType = a.chord.valid ? juce::String (a.chord.typeName) + (a.chord.inversion > 0 ? ", inv " + juce::String (a.chord.inversion) : juce::String())
                                     : juce::String (a.activeNotes > 0 ? "no confident chord" : "silence");
        st.transition = {};
        if (a.chordHistory.size() >= 2)
            st.transition = juce::String (a.chordHistory[a.chordHistory.size() - 2].chord.name) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 "))
                          + juce::String (a.chordHistory.back().chord.name) + "   vl " + juce::String (a.chordHistory.back().voiceLeadingSemitones);
        st.scale = a.scale.valid ? juce::String (a.scale.name) : juce::String ("-");
        juce::StringArray names;
        core.tracker().forEachActive ([&] (int, int n, const NoteSlot&) { if (names.size() < 8) names.add (juce::String (ChordDetector::pitchName (n % 12, false)) + juce::String (n / 12 - 1)); });
        st.notes = names.joinIntoString (" ");
        st.tempo = a.rhythm.source == BeatSource::None ? juce::String ("-") : juce::String (a.rhythm.bpm, 0) + " BPM";
    }

    const float sc = p.get (P::DashSize);
    const int style = p.geti (P::DashStyle);
    const float w = (style == 2 ? 190.0f : 250.0f) * sc;
    std::vector<int> panels;
    if (p.getb (P::DashChord)) panels.push_back (0);
    if (p.getb (P::DashScale)) panels.push_back (1);
    if (style != 2)
    {
        if (p.getb (P::DashNotes)) panels.push_back (2);
        if (p.getb (P::DashRhythm)) panels.push_back (3);
        if (p.getb (P::DashChannels)) panels.push_back (4);
        if (p.getb (P::DashControllers)) panels.push_back (5);
    }
    if (panels.empty()) return;
    auto panelH = [&] (int id)
    {
        if (style == 2) return 26.0f * sc;
        if (style == 1) return 22.0f * sc;
        switch (id) { case 0: return 76.0f * sc; case 1: return 44.0f * sc; case 2: return 74.0f * sc; case 3: return 58.0f * sc; case 4: return 40.0f * sc; default: return 56.0f * sc; }
    };
    float total = 0;
    for (int id : panels) total += panelH (id) + 6.0f;
    const int pos = p.geti (P::DashPos);
    const float x = (pos == 1 || pos == 3) ? canvas.getRight() - w - 14.0f : canvas.getX() + 14.0f;
    float y = pos >= 2 ? canvas.getBottom() - total - 10.0f : canvas.getY() + 14.0f;

    for (int id : panels)
    {
        const float h = panelH (id);
        auto r = juce::Rectangle<float> (x, y, w, h);
        g.setColour (juce::Colour (0xff07090c).withAlpha (0.55f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (colours::line2.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        r = r.reduced (10.0f * sc, 6.0f * sc);
        auto caps = [&] (const juce::String& t) { g.setColour (colours::text3); g.setFont (Fonts::get().caps (9.0f * sc)); g.drawText (t, r.removeFromTop (12.0f * sc), juce::Justification::centredLeft); };
        auto bar = [&] (float v, juce::Colour c)
        {
            auto b = r.removeFromBottom (3.0f * sc);
            g.setColour (colours::bg3.brighter (0.15f)); g.fillRoundedRectangle (b, 1.5f);
            g.setColour (c); g.fillRoundedRectangle (b.withWidth (b.getWidth() * saturate (v)), 1.5f);
        };
        if (style == 1 || style == 2)
        {
            g.setFont (Fonts::get().medium (12.0f * sc));
            juce::String text;
            switch (id)
            {
                case 0: text = "Chord  " + st.chord; break;
                case 1: text = "Scale  " + st.scale; break;
                case 2: text = juce::String ((int) std::lround (st.polyphony)) + " notes   " + st.notes; break;
                case 3: text = juce::String (st.density, 1) + " n/s   " + st.tempo; break;
                case 4: text = "Channel " + (core.lastChannel() >= 0 ? juce::String (core.lastChannel() + 1) : juce::String ("-")); break;
                default: text = "Bend " + juce::String (a.pitchBend[(size_t) std::max (0, a.lastChannel)], 2); break;
            }
            g.setColour (colours::text);
            g.drawText (text, r, juce::Justification::centredLeft, true);
        }
        else switch (id)
        {
            case 0:
                caps ("CHORD");
                g.setColour (a.chord.valid ? juce::Colours::white : colours::text3);
                g.setFont (Fonts::get().light (26.0f * sc));
                g.drawText (st.chord, r.removeFromTop (28.0f * sc), juce::Justification::centredLeft, true);
                g.setColour (colours::text2);
                g.setFont (Fonts::get().regular (10.5f * sc));
                g.drawText (st.chordType + (st.transition.isNotEmpty() ? "   " + st.transition : juce::String()), r.removeFromTop (14.0f * sc), juce::Justification::centredLeft, true);
                bar (st.chordConf, colours::accent);
                break;
            case 1:
                caps ("SCALE ESTIMATE");
                g.setColour (colours::text);
                g.setFont (Fonts::get().regular (13.0f * sc));
                g.drawText (st.scale, r.removeFromTop (16.0f * sc), juce::Justification::centredLeft, true);
                bar (st.scaleConf, colours::accent2);
                break;
            case 2:
            {
                caps ("NOTES");
                g.setColour (colours::text);
                g.setFont (Fonts::get().regular (11.5f * sc));
                g.drawText (juce::String ((int) std::lround (st.polyphony)) + " sounding  peak " + juce::String (a.peakPolyphony) + "   vel " + juce::String (st.velocity, 0),
                            r.removeFromTop (15.0f * sc), juce::Justification::centredLeft, true);
                g.setColour (colours::text2);
                g.setFont (Fonts::get().mono (10.5f * sc));
                g.drawText (st.notes.isEmpty() ? juce::String ("-") : st.notes, r.removeFromTop (14.0f * sc), juce::Justification::centredLeft, true);
                // mini keyboard (A0..C8)
                auto kb = r.removeFromBottom (14.0f * sc);
                const float kw = kb.getWidth() / 88.0f;
                for (int n = 21; n <= 108; ++n)
                {
                    const auto& slA = core.tracker();
                    bool on = false;
                    for (int ch = 0; ch < 16 && ! on; ++ch) on = slA.slot (ch, n).state != NoteState::Off;
                    const int pc = n % 12;
                    const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                    g.setColour (on ? colours::accent : (black ? colours::bg3 : colours::line2.brighter (0.2f)));
                    g.fillRect (kb.getX() + (float) (n - 21) * kw, kb.getY() + (black ? 0.0f : kb.getHeight() * 0.35f), std::max (1.0f, kw - 0.4f), black ? kb.getHeight() * 0.6f : kb.getHeight() * 0.65f);
                }
                break;
            }
            case 3:
            {
                caps ("RHYTHM");
                g.setColour (colours::text);
                g.setFont (Fonts::get().regular (11.5f * sc));
                g.drawText (juce::String (st.density, 1) + " notes/s   " + juce::String (st.onset, 1) + " onsets/s   " + st.tempo, r.removeFromTop (15.0f * sc), juce::Justification::centredLeft, true);
                auto line = r.removeFromTop (20.0f * sc);
                juce::Path contour;
                float lo = 127, hi = 0;
                for (float v : a.contourLine) if (v > 0) { lo = std::min (lo, v); hi = std::max (hi, v); }
                if (hi > lo)
                {
                    for (int i = 0; i < 64; ++i)
                    {
                        const float v = a.contourLine[(size_t) ((a.contourHead + i) % 64)];
                        const float px = line.getX() + line.getWidth() * (float) i / 63.0f;
                        const float py = v > 0 ? line.getBottom() - (v - lo) / (hi - lo) * line.getHeight() : line.getBottom();
                        if (i == 0) contour.startNewSubPath (px, py); else contour.lineTo (px, py);
                    }
                    g.setColour (colours::accent.withAlpha (0.8f));
                    g.strokePath (contour, juce::PathStrokeType (1.2f));
                }
                for (int b = 0; b < a.rhythm.beatsPerBar && a.rhythm.source != BeatSource::None; ++b)
                {
                    g.setColour (b == a.rhythm.beatInBar ? colours::warm : colours::line2);
                    g.fillEllipse (r.getRight() - (float) (a.rhythm.beatsPerBar - b) * 9.0f * sc, r.getY() - 18.0f * sc, 5.0f * sc, 5.0f * sc);
                }
                break;
            }
            case 4:
            {
                caps ("CHANNELS");
                const float bw = r.getWidth() / 16.0f;
                for (int c = 0; c < 16; ++c)
                {
                    const float v = a.channelActivity[(size_t) c];
                    g.setColour (colours::accent.withAlpha (0.15f + 0.85f * v));
                    g.fillRect (r.getX() + (float) c * bw + 1.0f, r.getBottom() - std::max (2.0f, r.getHeight() * v), bw - 2.0f, std::max (2.0f, r.getHeight() * v));
                }
                break;
            }
            default:
            {
                caps ("CONTROLLERS");
                const int ch = std::max (0, a.lastChannel);
                g.setColour (colours::text2);
                g.setFont (Fonts::get().regular (11.0f * sc));
                const auto& cc = a.recentCC[0];
                g.drawText ("Sustain " + juce::String (a.sustain[(size_t) ch] ? "ON" : "off") + "   pressure " + juce::String ((int) (a.pressure[(size_t) ch] * 127))
                            + (cc.channel >= 0 ? "   CC" + juce::String (cc.number) + "=" + juce::String (cc.value) : juce::String()),
                            r.removeFromTop (15.0f * sc), juce::Justification::centredLeft, true);
                auto b = r.removeFromTop (12.0f * sc);
                g.setColour (colours::bg3.brighter (0.15f));
                g.fillRoundedRectangle (b.withSizeKeepingCentre (b.getWidth(), 3.0f), 1.5f);
                const float bend = a.pitchBend[(size_t) ch];
                const float cx = b.getCentreX();
                g.setColour (colours::warm);
                g.fillRect (std::min (cx, cx + bend * b.getWidth() * 0.5f), b.getCentreY() - 1.5f, std::abs (bend) * b.getWidth() * 0.5f + 1.0f, 3.0f);
                g.fillEllipse (cx + bend * b.getWidth() * 0.5f - 3.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);
                break;
            }
        }
        y += h + 6.0f;
    }
}
} // namespace eidolon::ui
