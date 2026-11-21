// EIDOLON — Mode II: Constellation. Notes become nodes, musical relationships become connections and
// recognized chords become geometric structures; chord progressions leave evolving ghost structures.
#include "VisualWorld.h"
#include "../Intelligence/ChordDetector.h"
#include "../UI/Theme.h"
#include <algorithm>

namespace eidolon
{
Vec3 ConstellationLayer::nodePosition (const VisualNote& n, const FrameContext& f) const
{
    const float s = f.layerScale[1];
    const float a = (float) (((int) n.pitch * 7) % 12) / 12.0f * kTau + (n.seed - 0.5f) * 0.12f;
    Vec3 p;
    switch (f.p.geti (P::ConLayout))
    {
        case 1: p = { ((float) n.pitch - 60.0f) * 0.14f, ((float) n.velocity / 127.0f - 0.5f) * 4.0f + (n.seed - 0.5f) * 0.4f, ((float) n.channel - 7.5f) * 0.5f }; break;
        case 2:
        {
            const float r = 1.6f + ((float) n.pitch / 12.0f - 2.0f) * 0.55f;
            p = { r * std::cos (a), r * std::sin (a), (n.seed - 0.5f) * 0.6f };
            break;
        }
        case 3:
        {
            const float age = (float) (f.now - n.onTime);
            p = { 3.4f * std::cos (a), ((float) n.pitch - 60.0f) * 0.11f, 3.4f * std::sin (a) };
            p += Vec3 { noise1 (age * 0.3f + n.seed * 50.0f), noise1 (age * 0.25f + n.seed * 70.0f), noise1 (age * 0.2f + n.seed * 90.0f) } * std::min (2.5f, age * 0.6f);
            break;
        }
        default: p = { 3.4f * std::cos (a), ((float) n.pitch - 60.0f) * 0.11f, 3.4f * std::sin (a) }; break;
    }
    return p * s;
}

void ConstellationLayer::update (const FrameContext& f)
{
    const float dur = f.p.get (P::ConDuration);
    std::vector<Vec3> previousChord;
    for (const auto& nd : nodes)
        if (nd.n != nullptr && nd.n->alive && nd.n->sounding() && f.a.chordHistory.size() >= 2) previousChord.push_back (nd.p);

    nodes.clear();
    for (const auto& n : f.world.notes)
    {
        if (! n.alive) continue;
        const float a = n.life (f.now, dur);
        if (a <= 0.0f) continue;
        Node nd;
        nd.n = &n;
        nd.p = nodePosition (n, f);
        nd.a = a;
        nd.size = (0.12f + 0.16f * (float) n.velocity / 127.0f) * f.p.get (P::ConNodeSize) * f.layerScale[1];
        nd.c = juce::Colour (n.colour);
        nodes.push_back (nd);
    }
    if (nodes.size() > 160)
    {
        std::sort (nodes.begin(), nodes.end(), [] (const Node& x, const Node& y) { return x.n->onTime > y.n->onTime; });
        nodes.resize (160);
    }

    // chord progression ghosts
    if (f.a.chordChanged && f.a.chordHistory.size() >= 2)
    {
        const auto& prevEntry = f.a.chordHistory[f.a.chordHistory.size() - 2];
        if (prevEntry.chord.valid && previousChord.size() >= 2)
        {
            Ghost gh;
            gh.pts = previousChord;
            gh.c = f.pal.pitchClass (prevEntry.chord.root);
            gh.born = f.now;
            gh.name = prevEntry.chord.name;
            gh.shape = f.p.geti (P::ConChordShape);
            ghosts.push_back (std::move (gh));
            if (ghosts.size() > 6) ghosts.erase (ghosts.begin());
        }
    }
    ghosts.erase (std::remove_if (ghosts.begin(), ghosts.end(), [&] (const Ghost& g) { return f.now - g.born > 7.0; }), ghosts.end());
}

void ConstellationLayer::drawChord (RenderContext& rc, const FrameContext& f, const std::vector<Vec3>& pts, juce::Colour c, float a,
                                    int shape, const std::string& label, int rootIndex)
{
    if (pts.size() < 2 || a < 0.01f) return;
    Vec3 cen {};
    for (auto& p : pts) cen += p;
    cen *= 1.0f / (float) pts.size();
    const auto pc = rc.project (cen);
    if (! pc.visible) return;

    // order points by screen angle around the centroid
    std::vector<std::pair<float, int>> ang;
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const auto pr = rc.project (pts[(size_t) i]);
        ang.push_back ({ std::atan2 (pr.y - pc.y, pr.x - pc.x), i });
    }
    std::sort (ang.begin(), ang.end());
    scratch.clear();
    int rootSorted = -1;
    for (auto& e : ang) { if (e.second == rootIndex) rootSorted = (int) scratch.size(); scratch.push_back (pts[(size_t) e.second]); }
    rootIndex = rootSorted;
    const int n = (int) scratch.size();
    auto& g = rc.g;
    const float A = a * rc.alpha;
    const float spin = f.time * 0.35f;

    auto screenPoly = [&] (const std::vector<Vec3>& v, float scaleAbout)
    {
        juce::Path path;
        bool first = true;
        for (auto& p : v)
        {
            const auto pr = rc.project (cen + (p - cen) * scaleAbout);
            if (! pr.visible) continue;
            if (first) path.startNewSubPath (pr.x, pr.y); else path.lineTo (pr.x, pr.y);
            first = false;
        }
        path.closeSubPath();
        return path;
    };

    switch (shape)
    {
        case 0:   // polygon
        {
            auto path = screenPoly (scratch, 1.0f);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.12f * A)));
            g.fillPath (path);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.75f * A)));
            g.strokePath (path, juce::PathStrokeType (1.4f));
            break;
        }
        case 1:   // circle
        {
            float r = 0; for (auto& p : scratch) { const auto pr = rc.project (p); r += std::hypot (pr.x - pc.x, pr.y - pc.y); }
            r /= (float) n;
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.6f * A)));
            g.drawEllipse (pc.x - r, pc.y - r, 2 * r, 2 * r, 1.3f);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.07f * A)));
            g.fillEllipse (pc.x - r, pc.y - r, 2 * r, 2 * r);
            break;
        }
        case 2:   // radial
            for (int i = 0; i < n; ++i) rc.line3D (cen, scratch[(size_t) i], c, i == rootIndex ? 2.0f : 1.1f, A * (i == rootIndex ? 0.9f : 0.6f) / rc.alpha, true);
            rc.glow3D (cen, 0.6f * f.layerScale[1], c, 0.5f * a);
            break;
        case 3:   // linear (by pitch height)
        {
            auto byY = scratch;
            std::sort (byY.begin(), byY.end(), [] (const Vec3& x, const Vec3& y) { return x.y < y.y; });
            rc.polyline3D (byY.data(), (int) byY.size(), c, 1.6f, a * 0.8f, a * 0.8f);
            break;
        }
        case 4:   // orbital
            for (int i = 0; i < n; ++i)
            {
                const auto pr = rc.project (scratch[(size_t) i]);
                const float r = std::hypot (pr.x - pc.x, pr.y - pc.y);
                juce::Path e;
                e.addCentredArc (pc.x, pc.y, r, r * 0.45f, spin * (i % 2 ? 1.0f : -1.0f), 0.0f, kTau * 0.85f, true);
                g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.45f * A)));
                g.strokePath (e, juce::PathStrokeType (1.0f));
            }
            rc.glow3D (cen, 0.5f * f.layerScale[1], c, 0.6f * a);
            break;
        case 5:   // lattice (Tonnetz edges: thirds, fourths / fifths, sixths)
        case 6:   // graph (complete)
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                {
                    if (shape == 5)
                    {
                        const float dy = std::abs (scratch[(size_t) i].y - scratch[(size_t) j].y) / std::max (0.01f, 0.11f * f.layerScale[1]);
                        const int iv = (int) std::lround (dy) % 12;
                        if (! (iv == 3 || iv == 4 || iv == 5 || iv == 7 || iv == 8 || iv == 9)) continue;
                    }
                    rc.line3D (scratch[(size_t) i], scratch[(size_t) j], c, 1.0f, a * (shape == 5 ? 0.7f : 0.45f));
                }
            break;
        case 7:   // organic closed curve
        {
            juce::Path path;
            const int steps = 10;
            for (int i = 0; i <= n * steps; ++i)
            {
                const int k = (i / steps) % n;
                const float t = (float) (i % steps) / (float) steps;
                const Vec3& p0 = scratch[(size_t) ((k - 1 + n) % n)]; const Vec3& p1 = scratch[(size_t) k];
                const Vec3& p2 = scratch[(size_t) ((k + 1) % n)];     const Vec3& p3 = scratch[(size_t) ((k + 2) % n)];
                const float t2 = t * t, t3 = t2 * t;
                Vec3 q = (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
                q += (q - cen) * (0.08f * noise1 (f.time * 0.8f + (float) i * 0.13f));
                const auto pr = rc.project (q);
                if (i == 0) path.startNewSubPath (pr.x, pr.y); else path.lineTo (pr.x, pr.y);
            }
            path.closeSubPath();
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.1f * A)));
            g.fillPath (path);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.6f * A)));
            g.strokePath (path, juce::PathStrokeType (1.2f));
            break;
        }
        default:  // abstract star polygons
        {
            for (int i = 0; i < n; ++i) rc.line3D (scratch[(size_t) i], scratch[(size_t) ((i + 2) % n)], c, 1.0f, a * 0.65f);
            std::vector<Vec3> inner;
            for (auto& p : scratch) inner.push_back (cen + rotateY (p - cen, spin) * 0.45f);
            auto path = screenPoly (inner, 1.0f);
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.55f * A)));
            g.strokePath (path, juce::PathStrokeType (1.0f));
            break;
        }
    }

    if (! label.empty())
        rc.text3D (cen + Vec3 { 0, 0.9f * f.layerScale[1], 0 }, juce::String (label), c.interpolatedWith (juce::Colours::white, 0.5f), a, 13.0f);
}

void ConstellationLayer::render (RenderContext& rc, const FrameContext& f)
{
    const float intensity = f.p.get (P::ConIntensity);
    const float lineOpacity = f.p.get (P::ConOpacity);
    const float thickness = f.p.get (P::ConThickness);
    const float maxDist = f.p.get (P::ConDistance) * 5.0f * f.layerScale[1];
    const int geom = f.p.geti (P::ConGeometry);
    const bool simul = f.p.getb (P::ConSimul), harm = f.p.getb (P::ConHarmonic), pcl = f.p.getb (P::ConPitchClass);
    const bool oct = f.p.getb (P::ConOctave), custom = f.p.getb (P::ConCustom), seq = f.p.getb (P::ConSequential), voice = f.p.getb (P::ConVoice);
    const int customIv = f.p.geti (P::ConCustomIv);

    auto link = [&] (const Node& x, const Node& y, float weight)
    {
        const Vec3 d = y.p - x.p;
        const float len = d.length();
        if (len > maxDist || weight <= 0.0f) return;
        const float a = intensity * lineOpacity * std::min (x.a, y.a) * weight * (1.0f - 0.5f * len / std::max (0.01f, maxDist));
        if (a < 0.01f) return;
        const auto c = x.c.interpolatedWith (y.c, 0.5f);
        switch (geom)
        {
            case 0: rc.line3D (x.p, y.p, c, thickness, a); break;
            case 1:
            {
                Vec3 ctrl = (x.p + y.p) * 0.5f + Vec3 { 0, len * 0.22f, 0 } + (x.p + y.p).normalised() * (len * 0.12f);
                Vec3 prev = x.p;
                for (int i = 1; i <= 8; ++i)
                {
                    const float t = (float) i / 8.0f, u = 1.0f - t;
                    const Vec3 q = x.p * (u * u) + ctrl * (2 * u * t) + y.p * (t * t);
                    rc.line3D (prev, q, c, thickness, a);
                    prev = q;
                }
                break;
            }
            case 2:
                for (int i = 0; i < 8; i += 2) rc.line3D (lerpv (x.p, y.p, (float) i / 8.0f), lerpv (x.p, y.p, (float) (i + 1) / 8.0f), c, thickness, a);
                break;
            default: rc.line3D (x.p, y.p, c, thickness, a, true); break;
        }
    };

    const int n = (int) nodes.size();
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
        {
            const auto& x = nodes[(size_t) i];
            const auto& y = nodes[(size_t) j];
            const int d = std::abs ((int) x.n->pitch - (int) y.n->pitch);
            const int iv = d % 12;
            const bool bothSounding = x.n->sounding() && y.n->sounding();
            const bool near = std::abs (x.n->onTime - y.n->onTime) < 1.2 || bothSounding;
            float w = 0.0f;
            if (simul && (bothSounding || std::abs (x.n->onTime - y.n->onTime) < 0.06)) w = std::max (w, 1.0f);
            if (harm && near && (iv == 3 || iv == 4 || iv == 5 || iv == 7 || iv == 8 || iv == 9)) w = std::max (w, (iv == 5 || iv == 7) ? 0.8f : 0.6f);
            if (pcl && iv == 0) w = std::max (w, 0.55f);
            if (oct && d == 12) w = std::max (w, 0.65f);
            if (custom && iv == customIv && near) w = std::max (w, 0.6f);
            if (w > 0.0f) link (x, y, w);
        }

    if (seq || voice)
    {
        std::vector<const Node*> byTime;
        for (auto& nd : nodes) byTime.push_back (&nd);
        std::sort (byTime.begin(), byTime.end(), [] (const Node* a, const Node* b) { return a->n->onTime < b->n->onTime; });
        for (size_t i = 1; seq && i < byTime.size(); ++i)
            if (byTime[i]->n->onTime - byTime[i - 1]->n->onTime < 1.5) link (*byTime[i - 1], *byTime[i], 0.75f);
        if (voice)
            for (int v = 0; v < 8; ++v)
            {
                const Node* last = nullptr;
                for (auto* nd : byTime)
                    if (nd->n->voice == v) { if (last != nullptr) link (*last, *nd, 0.7f); last = nd; }
            }
    }

    // ghosts of previous chords drift away
    for (const auto& gh : ghosts)
    {
        const float age = (float) (f.now - gh.born);
        std::vector<Vec3> moved;
        for (auto p : gh.pts) moved.push_back (p * (1.0f - age * 0.05f) + Vec3 { 0, age * 0.15f, -age * 0.9f } * f.layerScale[1]);
        drawChord (rc, f, moved, gh.c, 0.5f * (1.0f - age / 7.0f), gh.shape, age < 2.5f ? gh.name : std::string(), -1);
    }

    // nodes
    for (const auto& nd : nodes)
    {
        rc.glow3D (nd.p, nd.size * (3.0f + 3.0f * rc.bloom), nd.c, 0.35f * nd.a);
        rc.sphere (nd.p, nd.size * 0.5f, nd.c.brighter (0.2f), nd.a, Material::Luminous, f.time);
        if (nd.n->sounding())
        {
            const auto pr = rc.project (nd.p);
            if (pr.visible)
            {
                const float r = nd.size * pr.scale * 1.4f;
                rc.g.setColour (nd.c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.55f * rc.alpha)));
                rc.g.drawEllipse (pr.x - r, pr.y - r, 2 * r, 2 * r, 1.0f);
            }
        }
    }

    // current chord geometry
    if (f.p.getb (P::ConChord) && f.a.chord.valid)
    {
        std::vector<Vec3> pts;
        int rootIndex = -1;
        std::array<bool, 12> used {};
        for (const auto& nd : nodes)
        {
            if (! nd.n->sounding()) continue;
            const int pc = nd.n->pitch % 12;
            if (! (f.a.chord.pitchClasses & (1u << pc)) || used[(size_t) pc]) continue;
            used[(size_t) pc] = true;
            if (pc == f.a.chord.root) rootIndex = (int) pts.size();
            pts.push_back (nd.p);
        }
        drawChord (rc, f, pts, f.pal.pitchClass (f.a.chord.root), 0.9f * std::max (0.4f, f.a.chord.confidence),
                   f.p.geti (P::ConChordShape), f.a.chord.name, rootIndex);
    }
}
} // namespace eidolon
