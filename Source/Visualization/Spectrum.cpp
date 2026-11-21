// EIDOLON — Mode V: Spectrum. MIDI statistics (not an audio FFT): pitch, pitch class, octave, velocity and
// duration distributions, a pitch-class heatmap and polyphony / density timelines, over a selectable window.
#include "VisualWorld.h"
#include "../Intelligence/ChordDetector.h"
#include "../UI/Theme.h"

namespace eidolon
{
namespace
{
const char* kViewNames[] = { "PITCH DISTRIBUTION", "PITCH-CLASS DISTRIBUTION", "OCTAVE DISTRIBUTION", "VELOCITY DISTRIBUTION",
                             "DURATION DISTRIBUTION", "PITCH-CLASS HEATMAP", "POLYPHONY TIMELINE", "DENSITY TIMELINE" };
const char* kWindowNames[] = { "instant", "1 s", "5 s", "10 s", "30 s", "entire capture", "custom" };

template <size_t N> int fill (std::array<float, 128>& dst, const std::array<float, N>& src, int from = 0, int count = (int) N)
{
    float mx = 1e-6f;
    for (int i = 0; i < count; ++i) mx = std::max (mx, src[(size_t) (from + i)]);
    for (int i = 0; i < count; ++i) dst[(size_t) i] = src[(size_t) (from + i)] / mx;
    return count;
}
} // namespace

void SpectrumLayer::update (const FrameContext& f)
{
    const int view = f.p.geti (P::SpecView);
    std::array<float, 128> target {};
    const auto& d = f.statsWindow == std::numeric_limits<double>::infinity() ? f.stats.entire() : f.stats.windowed();
    switch (view)
    {
        case 0: fill (target, d.pitch, 21, 88); break;
        case 1: fill (target, d.pitchClass); break;
        case 2: fill (target, d.octave); break;
        case 3: fill (target, d.velocity); break;
        case 4: fill (target, d.duration); break;
        case 6: case 7:
        {
            const auto& line = view == 6 ? f.stats.polyphonyTimeline() : f.stats.densityTimeline();
            const int head = f.stats.timelineHead();
            float mx = view == 6 ? 8.0f : 12.0f;
            for (float v : line) mx = std::max (mx, v);
            for (int i = 0; i < 120; ++i) target[(size_t) i] = line[(size_t) ((head + 120 + i * 1) % MusicalStatistics::kTimelineLength)] / mx;
            break;
        }
        default: break;
    }
    const float k = view != lastView ? 1.0f : smoothCoef (f.dt, 0.02f + 0.5f * f.p.get (P::SpecSmooth));
    for (size_t i = 0; i < bars.size(); ++i) bars[i] += (target[i] - bars[i]) * k;

    if (view == 5)
    {
        const auto& h = f.stats.pitchClassHeat();
        float mx = 1e-6f;
        for (auto& col : h) for (float v : col) mx = std::max (mx, v);
        for (int c = 0; c < 48; ++c)
            for (int pc = 0; pc < 12; ++pc)
            {
                auto& cell = heat[(size_t) (c * 12 + pc)];
                cell += (h[(size_t) c][(size_t) pc] / mx - cell) * k;
            }
    }
    lastView = view;
}

void SpectrumLayer::render (RenderContext& rc, const FrameContext& f)
{
    const int view = f.p.geti (P::SpecView);
    const int placement = f.p.geti (P::SpecPlacement);
    const float glowK = f.p.get (P::SpecGlow);
    const float s = f.layerScale[4];
    auto& g = rc.g;
    const auto area = rc.area;
    const int counts[] = { 88, 12, 11, 16, 12, 48, 120, 120 };
    const int n = counts[clampv (view, 0, 7)];

    // ---------------------------------------------------------------- 3D floor
    if (placement == 0 && view != 5)
    {
        const float floorY = -6.5f * s, width = 22.0f * s;
        const bool radial = view == 1;
        for (int i = 0; i < n; ++i)
        {
            const float v = bars[(size_t) i];
            const auto c = view == 1 ? f.pal.pitchClass (i) : view == 0 ? f.pal.pitchClass ((i + 21) % 12) : f.pal.at ((float) i / (float) n);
            Vec3 base;
            if (radial) { const float a = (float) ((i * 7) % 12) / 12.0f * kTau; base = { std::cos (a) * 5.0f * s, floorY, std::sin (a) * 5.0f * s }; }
            else base = { ((float) i / (float) (n - 1) - 0.5f) * width, floorY, -3.0f * s };
            const Vec3 top = base + Vec3 { 0, (0.05f + v * 6.0f) * s, 0 };
            if (view >= 6 && i > 0)
            {
                const float v0 = bars[(size_t) i - 1];
                const Vec3 prevTop { ((float) (i - 1) / (float) (n - 1) - 0.5f) * width, floorY + (0.05f + v0 * 6.0f) * s, -3.0f * s };
                rc.line3D (prevTop, top, c, 1.6f, 0.9f, true);
                continue;
            }
            rc.line3D (base, top, c, radial ? 5.0f : std::max (1.5f, 9.0f * rc.cam.focalLength() / 900.0f * (view == 0 ? 0.35f : 1.0f)), 0.35f + 0.6f * v, glowK > 0.3f);
            if (v > 0.02f) rc.glow3D (top, (0.25f + 0.4f * v) * s, c, 0.4f * glowK * v);
        }
        rc.text3D ({ 0, floorY - 0.7f * s, -3.0f * s }, juce::String (kViewNames[view]) + "  -  " + kWindowNames[f.p.geti (P::SpecWindow)],
                   ui::colours::text2, 0.8f, 11.0f);
        return;
    }

    // ---------------------------------------------------------------- 2D panel / fullscreen
    juce::Rectangle<float> r = placement == 2 ? area.reduced (area.getWidth() * 0.12f, area.getHeight() * 0.16f)
                                              : juce::Rectangle<float> (area.getX() + area.getWidth() * 0.18f, area.getBottom() - area.getHeight() * 0.3f,
                                                                        area.getWidth() * 0.64f, area.getHeight() * 0.22f);
    r = r.withSizeKeepingCentre (r.getWidth() * std::min (1.4f, s), r.getHeight() * std::min (1.4f, s));
    const float A = rc.alpha;
    g.setColour (ui::colours::text3.withAlpha (0.8f * A));
    g.setFont (ui::Fonts::get().caps (10.5f));
    g.drawText (juce::String (kViewNames[view]) + "   " + juce::String (kWindowNames[f.p.geti (P::SpecWindow)]).toUpperCase(),
                r.withHeight (16.0f).translated (0, -22.0f), juce::Justification::centredLeft);
    g.setColour (ui::colours::line2.withAlpha (0.6f * A));
    g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());

    if (view == 1)
    {
        const float rad = std::min (r.getWidth(), r.getHeight() * (placement == 2 ? 1.0f : 1.6f)) * 0.5f;
        const auto c0 = r.getCentre();
        for (int i = 0; i < 12; ++i)
        {
            const float a0 = (float) i / 12.0f * kTau - kPi * 0.5f - kTau / 24.0f;
            const float v = bars[(size_t) ((i * 7) % 12)];
            juce::Path wedge;
            wedge.addPieSegment (c0.x - rad, c0.y - rad, 2 * rad, 2 * rad, a0 + kPi * 0.5f, a0 + kTau / 12.0f + kPi * 0.5f - 0.02f, 0.25f);
            g.setColour (ui::colours::bg3.withAlpha (0.35f * A));
            g.fillPath (wedge);
            juce::Path fillW;
            const float rr = rad * (0.25f + 0.75f * v);
            fillW.addPieSegment (c0.x - rr, c0.y - rr, 2 * rr, 2 * rr, a0 + kPi * 0.5f, a0 + kTau / 12.0f + kPi * 0.5f - 0.02f, 0.25f * rad / std::max (1.0f, rr));
            const auto c = f.pal.pitchClass ((i * 7) % 12);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, (0.25f + 0.7f * v) * A)));
            g.fillPath (fillW);
            const float la = a0 + kTau / 24.0f;
            g.setColour (ui::colours::text2.withAlpha (A));
            g.setFont (ui::Fonts::get().medium (11.0f));
            g.drawText (ChordDetector::pitchName ((i * 7) % 12, false), juce::Rectangle<float> (c0.x + std::cos (la) * (rad + 14) - 14, c0.y + std::sin (la) * (rad + 14) - 8, 28, 16),
                        juce::Justification::centred);
        }
        return;
    }
    if (view == 5)
    {
        const float cw = r.getWidth() / 48.0f, ch = r.getHeight() / 12.0f;
        for (int c = 0; c < 48; ++c)
            for (int pc = 0; pc < 12; ++pc)
            {
                const float v = heat[(size_t) (c * 12 + pc)];
                if (v < 0.02f) continue;
                g.setColour (f.pal.pitchClass (pc).withAlpha (juce::jlimit (0.0f, 1.0f, (0.15f + 0.85f * v) * A)));
                g.fillRect (r.getX() + (float) c * cw, r.getBottom() - (float) (pc + 1) * ch, cw - 1.0f, ch - 1.0f);
            }
        return;
    }
    if (view >= 6)
    {
        juce::Path line, area2;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1), y = r.getBottom() - r.getHeight() * bars[(size_t) i];
            if (i == 0) { line.startNewSubPath (x, y); area2.startNewSubPath (x, r.getBottom()); }
            else line.lineTo (x, y);
            area2.lineTo (x, y);
        }
        area2.lineTo (r.getRight(), r.getBottom());
        area2.closeSubPath();
        const auto c = f.pal.at (view == 6 ? 0.2f : 0.6f);
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.25f * A), r.getX(), r.getY(), c.withAlpha (0.0f), r.getX(), r.getBottom(), false));
        g.fillPath (area2);
        g.setColour (c.withAlpha (0.9f * A));
        g.strokePath (line, juce::PathStrokeType (1.5f));
        return;
    }
    const float bw = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float v = bars[(size_t) i];
        const auto c = view == 0 ? f.pal.pitchClass ((i + 21) % 12) : f.pal.at ((float) i / (float) n);
        const auto bar = juce::Rectangle<float> (r.getX() + (float) i * bw + 1.0f, r.getBottom() - r.getHeight() * v, std::max (1.0f, bw - 2.0f), r.getHeight() * v);
        if (glowK > 0.05f && v > 0.05f) rc.glow (bar.getCentreX(), bar.getY(), bw * 2.0f + 6.0f, c, 0.25f * glowK);
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.95f * A), bar.getX(), bar.getY(), c.withAlpha (0.25f * A), bar.getX(), bar.getBottom(), false));
        g.fillRect (bar);
    }
    if (view == 4)
    {
        static const char* lab[] = { "<31ms", "62", "125", "250", "500ms", "1s", "2", "4", "8", "16", "32", ">" };
        g.setFont (ui::Fonts::get().regular (9.5f));
        g.setColour (ui::colours::text3.withAlpha (A));
        for (int i = 0; i < 12; ++i) g.drawText (lab[i], juce::Rectangle<float> (r.getX() + (float) i * bw, r.getBottom() + 3, bw, 12), juce::Justification::centred);
    }
}
} // namespace eidolon
