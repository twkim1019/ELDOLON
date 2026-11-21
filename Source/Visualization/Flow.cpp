// EIDOLON — Mode III: Flow. Melodic movement as continuous trajectories, one per tracked voice.
// Leaps flare, repeated notes ring, ascending lines warm up, descending lines cool down.
#include "VisualWorld.h"
#include "../UI/Theme.h"
#include <algorithm>

namespace eidolon
{
void FlowLayer::update (const FrameContext& f)
{
    const int maxVoices = f.p.geti (P::FlowMaxVoices);
    for (const auto& c : f.changes)
    {
        if (c.kind != NoteChange::Start) continue;
        const int v = clampv (c.voice < 0 ? 0 : c.voice, 0, 7) % std::max (1, maxVoices);
        auto& line = voices[(size_t) v];
        const int interval = line.empty() ? 0 : (int) c.pitch - (int) std::lround (line.back().pitch);
        line.push_back ({ std::min (c.time, f.now), (float) c.pitch, (float) c.velocity / 127.0f, interval });
        if (line.size() > 400) line.erase (line.begin(), line.begin() + 100);
    }
    const double keep = f.p.get (P::FlowHistory);
    for (auto& line : voices)
        while (line.size() > 1 && f.now - line[1].t > keep) line.erase (line.begin());
}

void FlowLayer::render (RenderContext& rc, const FrameContext& f)
{
    const float s = f.layerScale[2];
    const float speed = f.p.get (P::FlowSpeed) * 1.6f;
    const float width = f.p.get (P::FlowWidth);
    const float smooth = f.p.get (P::FlowSmooth);
    const float leapK = f.p.get (P::FlowLeap);
    const float velWidth = f.p.get (P::FlowVelWidth);
    const int style = f.p.geti (P::FlowStyle);
    const int maxVoices = f.p.geti (P::FlowMaxVoices);
    const double keep = f.p.get (P::FlowHistory);
    const int colorBy = f.p.geti (P::LookColorBy);

    for (int v = 0; v < maxVoices; ++v)
    {
        const auto& line = voices[(size_t) v];
        if (line.empty()) continue;
        const float z = ((float) v - (float) (maxVoices - 1) * 0.5f) * 1.1f;
        auto toWorld = [&] (double t, float pitch) { return Vec3 { (float) (t - f.now) * speed, (pitch - 60.0f) * 0.16f, z } * s; };
        const juce::Colour base = colorBy == 4 || colorBy == 1 ? f.pal.at ((float) v / 8.0f * 1.6f + 0.05f) : f.pal.at ((float) v * 0.13f + 0.1f);

        // build a smooth trajectory: hold each pitch, glide into the next with a smoothed ramp
        pts.clear(); widths.clear();
        const int steps = 3 + (int) (6.0f * f.motionQuality);
        for (size_t i = 0; i < line.size(); ++i)
        {
            const auto& a = line[i];
            const double tNext = i + 1 < line.size() ? line[i + 1].t : f.now;
            const float pNext = i + 1 < line.size() ? line[i + 1].pitch : a.pitch;
            const double glide = std::min ((tNext - a.t) * (0.15 + 0.6 * smooth), 0.35);
            const double holdEnd = tNext - glide;
            pts.push_back (toWorld (a.t, a.pitch));
            widths.push_back (0.18f * width * (1.0f + velWidth * (a.vel - 0.5f)) * s);
            if (holdEnd > a.t) { pts.push_back (toWorld (holdEnd, a.pitch)); widths.push_back (widths.back()); }
            if (i + 1 < line.size())
                for (int k = 1; k < steps; ++k)
                {
                    const float u = (float) k / (float) steps;
                    const float e = smooth > 0.01f ? lerpf (u, smoothstepf (0, 1, u), smooth) : u;
                    pts.push_back (toWorld (holdEnd + glide * u, lerpf (a.pitch, pNext, e)));
                    widths.push_back (widths.back());
                }
        }
        if (pts.empty()) continue;
        const int n = (int) pts.size();
        const float age0 = (float) (f.now - line.front().t);
        const float fadeOld = saturate (1.0f - age0 / (float) keep) * 0.1f;
        const float lastVel = line.back().vel;
        const juce::Colour dir = line.back().interval > 0 ? base.withRotatedHue (-0.04f).brighter (0.15f)
                               : line.back().interval < 0 ? base.withRotatedHue (0.05f) : base;

        switch (style)
        {
            case 0:   // ribbon
                rc.ribbon3D (pts.data(), widths.data(), n, dir.withMultipliedBrightness (0.9f), fadeOld, 0.55f);
                rc.polyline3D (pts.data(), n, dir.brighter (0.45f), 1.2f, fadeOld, 0.95f);
                break;
            case 1:   // line
                rc.polyline3D (pts.data(), n, dir.brighter (0.2f), 1.6f * width, fadeOld, 1.0f);
                break;
            case 2:   // curve + note dots
                rc.polyline3D (pts.data(), n, dir, 1.3f * width, fadeOld, 0.9f);
                for (const auto& p : line) rc.glow3D (toWorld (p.t, p.pitch), 0.25f * s * width, dir, 0.6f * (0.3f + p.vel));
                break;
            case 3:   // particles along the path
                for (int i = 0; i < n; i += 1)
                {
                    const float t = (float) i / (float) std::max (1, n - 1);
                    const Vec3 j { 0, noise1 ((float) i * 0.7f + f.time * 2.0f) * 0.08f, noise1 ((float) i * 0.9f - f.time * 1.5f) * 0.08f };
                    rc.glow3D (pts[(size_t) i] + j * s, 0.12f * s * width, dir, 0.15f + 0.6f * t);
                }
                break;
            case 4:   // fluid: three braided strands
                for (int strand = 0; strand < 3; ++strand)
                {
                    std::vector<Vec3> q (pts);
                    for (int i = 0; i < n; ++i)
                    {
                        const float ph = (float) i * 0.18f + f.time * 1.3f + (float) strand * 2.1f;
                        q[(size_t) i] += Vec3 { 0, std::sin (ph) * 0.12f, std::cos (ph * 0.8f) * 0.18f } * (s * width);
                    }
                    rc.polyline3D (q.data(), n, dir.withRotatedHue (0.03f * (float) strand), 1.1f, fadeOld, 0.75f);
                }
                break;
            default:  // geometric: angular segments with polygon markers
            {
                std::vector<Vec3> q;
                for (const auto& p : line) q.push_back (toWorld (p.t, p.pitch));
                q.push_back (toWorld (f.now, line.back().pitch));
                rc.polyline3D (q.data(), (int) q.size(), dir, 1.4f * width, fadeOld, 1.0f);
                for (size_t i = 0; i < line.size(); ++i)
                {
                    const auto pr = rc.project (q[i]);
                    if (! pr.visible) continue;
                    const float r = 0.14f * s * pr.scale * (0.6f + line[i].vel);
                    juce::Path tri;
                    tri.addPolygon ({ pr.x, pr.y }, 3 + (std::abs (line[i].interval) % 4), r, f.time * 0.5f);
                    rc.g.setColour (dir.withAlpha (juce::jlimit (0.0f, 1.0f, 0.7f * rc.alpha)));
                    rc.g.strokePath (tri, juce::PathStrokeType (1.0f));
                }
                break;
            }
        }

        // head, leaps and repeated notes
        const Vec3 head = toWorld (f.now, line.back().pitch);
        rc.glow3D (head, (0.5f + 0.6f * lastVel) * s * width, dir, 0.55f);
        rc.sphere (head, 0.09f * s * (0.7f + width * 0.3f), dir.brighter (0.4f), 0.95f, Material::Luminous, f.time);
        for (const auto& p : line)
        {
            const float age = (float) (f.now - p.t);
            if (age > 1.5f) continue;
            const Vec3 at = toWorld (p.t, p.pitch);
            if (std::abs (p.interval) >= 7 && leapK > 0.01f)
                rc.glow3D (at, (0.6f + 0.08f * (float) std::abs (p.interval)) * s * leapK, base.brighter (0.5f), 0.55f * (1.0f - age / 1.5f));
            else if (p.interval == 0 && &p != &line.front())
            {
                const auto pr = rc.project (at);
                if (pr.visible)
                {
                    const float r = (0.15f + age * 0.5f) * s * pr.scale;
                    rc.g.setColour (base.withAlpha (juce::jlimit (0.0f, 1.0f, 0.5f * (1.0f - age / 1.5f) * rc.alpha)));
                    rc.g.drawEllipse (pr.x - r, pr.y - r, 2 * r, 2 * r, 1.0f);
                }
            }
        }
    }
}
} // namespace eidolon
