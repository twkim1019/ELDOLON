// EIDOLON — the ten procedural cinematic environments. Each scene is generated every frame from musical
// inputs (energy, density, pitch-class activity, contour, chord changes, beat phase, ripples): no static images.
#include "SceneManager.h"

namespace eidolon
{
namespace
{
float h01 (float i) { return (hash11 (i * 1.731f + 0.37f) + 1.0f) * 0.5f; }

struct Ctx
{
    RenderContext& rc;
    const SceneInputs& in;
    const SceneDrawParams& dp;
    const Palette& pal;
    Vec3 tr (const Vec3& v) const { return rotateY (v * dp.scale, dp.rotation); }
    bool keep (int i) const { return dp.dissolve <= 0.001f || h01 ((float) i * 3.17f + 11.0f) >= dp.dissolve; }
    float k() const { return in.intensity; }
};

void stars (const Ctx& c, int count, float radius, float bright)
{
    const float tw = c.in.time;
    for (int i = 0; i < count; ++i)
    {
        if (! c.keep (i)) continue;
        const float u = h01 ((float) i), v = h01 ((float) i + 0.5f);
        const float th = u * kTau, ph = std::acos (2.0f * v - 1.0f);
        const Vec3 p { std::sin (ph) * std::cos (th) * radius, std::cos (ph) * radius * 0.6f, std::sin (ph) * std::sin (th) * radius };
        const auto pr = c.rc.project (c.tr (p));
        if (! pr.visible) continue;
        const float tws = 0.6f + 0.4f * std::sin (tw * (0.5f + h01 ((float) i + 9.0f) * 2.0f) + (float) i);
        const float a = bright * tws * (0.25f + 0.75f * h01 ((float) i + 3.3f));
        c.rc.point (pr.x, pr.y, h01 ((float) i + 7.7f) > 0.97f ? 1.3f : 0.7f, c.pal.at (h01 ((float) i + 1.1f)).interpolatedWith (juce::Colours::white, 0.7f), a);
    }
}

void ringXZ (const Ctx& c, const Vec3& centre, float r, float tilt, juce::Colour col, float a, int segs)
{
    Vec3 prev;
    for (int i = 0; i <= segs; ++i)
    {
        const float t = (float) i / (float) segs * kTau;
        const Vec3 q = c.tr (centre + rotateX (Vec3 { std::cos (t) * r, 0, std::sin (t) * r }, tilt));
        if (i > 0) c.rc.line3D (prev, q, col, 0.9f, a);
        prev = q;
    }
}

// ------------------------------------------------------------------------------------------------- scenes
void nebula (const Ctx& c)
{
    stars (c, (int) (380 * c.in.detail) + 60, 70.0f, 0.5f);
    const int blobs = 20 + (int) (26 * c.in.detail);
    for (int i = 0; i < blobs; ++i)
    {
        if (! c.keep (i)) continue;
        const float u = h01 ((float) i * 2.1f), v = h01 ((float) i * 3.7f), w = h01 ((float) i * 5.3f);
        const float th = u * kTau, ph = 0.35f + v * 2.4f;
        Vec3 p { std::sin (ph) * std::cos (th), std::cos (ph) * 0.55f, std::sin (ph) * std::sin (th) };
        p = p * (20.0f + 16.0f * w);
        p += Vec3 { noise1 (c.in.time * 0.03f + (float) i), noise1 (c.in.time * 0.025f + (float) i * 1.3f), noise1 (c.in.time * 0.02f + (float) i * 2.1f) } * 3.0f;
        const float r = (7.0f + 10.0f * h01 ((float) i * 7.1f)) * (1.0f + 0.35f * c.in.energy);
        c.rc.glow3D (c.tr (p), r, c.pal.at (h01 ((float) i * 1.9f) + c.in.time * 0.006f), (0.05f + 0.08f * c.in.energy + 0.04f * c.in.density) * c.k(), 1800.0f);
    }
    for (int pc = 0; pc < 12; ++pc)
    {
        const float act = c.in.pcActivity[(size_t) pc];
        if (act < 0.02f) continue;
        const float a = (float) ((pc * 7) % 12) / 12.0f * kTau;
        c.rc.glow3D (c.tr ({ std::cos (a) * 18.0f, 4.0f * std::sin (a * 2.0f), std::sin (a) * 18.0f - 6.0f }), 6.0f + 4.0f * act, c.pal.pitchClass (pc), 0.18f * act * c.k(), 1400.0f);
    }
}

void cathedral (const Ctx& c)
{
    const auto stone = c.pal.at (0.55f).interpolatedWith (juce::Colours::white, 0.45f);
    for (int k = 0; k < 10; ++k)
    {
        const float z = 8.0f - (float) k * 5.0f;
        for (int side = -1; side <= 1; side += 2)
        {
            if (! c.keep (k * 2 + (side > 0))) continue;
            const int pc = (k * 2 + (side > 0 ? 1 : 0)) % 12;
            const float act = c.in.pcActivity[(size_t) pc];
            const float a = (0.1f + 0.55f * act + 0.12f * c.in.chordPulse) * c.k();
            const auto col = act > 0.05f ? c.pal.pitchClass (pc) : stone;
            const Vec3 base { (float) side * 7.0f, -7.0f, z }, top { (float) side * 7.0f, 7.0f, z };
            c.rc.line3D (c.tr (base), c.tr (top), col, 2.0f, a, true);
            c.rc.glow3D (c.tr (top), 1.2f, col, a * 0.8f);
            if (k > 0) c.rc.line3D (c.tr (top), c.tr ({ (float) side * 7.0f, 7.0f, z + 5.0f }), stone, 1.0f, 0.08f * c.k());
        }
        // pointed gothic arch
        Vec3 prevL = c.tr ({ -7.0f, 7.0f, z }), prevR = c.tr ({ 7.0f, 7.0f, z });
        for (int i = 1; i <= 10; ++i)
        {
            const float t = (float) i / 10.0f, u = 1.0f - t;
            const Vec3 l = Vec3 { -7, 7, z } * (u * u) + Vec3 { -6.5f, 11.0f, z } * (2 * u * t) + Vec3 { 0, 12.0f, z } * (t * t);
            const Vec3 r = Vec3 { 7, 7, z } * (u * u) + Vec3 { 6.5f, 11.0f, z } * (2 * u * t) + Vec3 { 0, 12.0f, z } * (t * t);
            c.rc.line3D (prevL, c.tr (l), stone, 1.0f, 0.14f * c.k());
            c.rc.line3D (prevR, c.tr (r), stone, 1.0f, 0.14f * c.k());
            prevL = c.tr (l); prevR = c.tr (r);
        }
    }
    // rose window
    const Vec3 rose { 0, 4.0f, -46.0f };
    const auto rc = c.in.chordRoot >= 0 ? c.pal.pitchClass (c.in.chordRoot) : c.pal.at (0.3f);
    const float glow = (0.2f + 0.6f * c.in.chordPulse + 0.2f * c.in.energy) * c.k();
    for (int ring = 1; ring <= 3; ++ring) ringXZ (c, rose, 1.7f * (float) ring, kPi * 0.5f, rc, glow * 0.5f, 48);
    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i / 12.0f * kTau + c.in.time * 0.02f;
        c.rc.line3D (c.tr (rose), c.tr (rose + Vec3 { std::cos (a) * 5.1f, std::sin (a) * 5.1f, 0 }), rc, 1.0f, glow * (0.3f + 0.7f * c.in.pcActivity[(size_t) i]));
    }
    c.rc.glow3D (c.tr (rose), 9.0f, rc, glow * 0.4f, 1600.0f);
    for (int i = -4; i <= 4; ++i) c.rc.line3D (c.tr ({ (float) i * 1.75f, -7.0f, 10.0f }), c.tr ({ (float) i * 1.75f, -7.0f, -48.0f }), stone, 0.8f, 0.05f * c.k());
}

void aurora (const Ctx& c)
{
    stars (c, (int) (220 * c.in.detail) + 40, 70.0f, 0.4f);
    const float step = 1.6f / std::max (0.35f, c.in.detail);
    for (int k = 0; k < 5; ++k)
    {
        if (! c.keep (k)) continue;
        const float z = -12.0f - (float) k * 6.0f;
        const auto col = c.pal.at (0.08f + (float) k * 0.13f + c.in.time * 0.004f);
        juce::Path curtain, edge;
        std::vector<juce::Point<float>> tops, bots;
        float minY = 1e9f, maxY = -1e9f;
        for (float x = -34.0f; x <= 34.0f; x += step)
        {
            const float yt = 5.0f + 3.0f * noise1 (x * 0.07f + c.in.time * 0.12f + (float) k * 10.0f) + c.in.contour * 1.5f + c.in.energy * 2.0f;
            const float yb = yt - (5.0f + 3.0f * noise1 (x * 0.05f - c.in.time * 0.09f + (float) k * 4.0f));
            const float wave = std::sin (x * 0.12f + c.in.time * 0.3f + (float) k) * 1.5f;
            const auto pt = c.rc.project (c.tr ({ x, yt, z + wave })), pb = c.rc.project (c.tr ({ x, yb, z + wave }));
            if (! pt.visible || ! pb.visible) continue;
            tops.push_back ({ pt.x, pt.y }); bots.push_back ({ pb.x, pb.y });
            minY = std::min (minY, pt.y); maxY = std::max (maxY, pb.y);
        }
        if (tops.size() < 2) continue;
        curtain.startNewSubPath (tops[0]);
        for (auto& p : tops) curtain.lineTo (p);
        for (size_t i = bots.size(); i-- > 0;) curtain.lineTo (bots[i]);
        curtain.closeSubPath();
        const float a = (0.16f + 0.2f * c.in.energy) * c.k() * c.rc.alpha;
        juce::ColourGradient grad (col.withAlpha (0.0f), 0, minY, col.withAlpha (0.0f), 0, maxY, false);
        grad.addColour (0.55, col.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        grad.addColour (0.85, col.brighter (0.4f).withAlpha (juce::jlimit (0.0f, 1.0f, a * 1.3f)));
        c.rc.g.setGradientFill (grad);
        c.rc.g.fillPath (curtain);
        edge.startNewSubPath (bots[0]);
        for (auto& p : bots) edge.lineTo (p);
        c.rc.g.setColour (col.brighter (0.5f).withAlpha (juce::jlimit (0.0f, 1.0f, a * 1.2f)));
        c.rc.g.strokePath (edge, juce::PathStrokeType (1.2f));
    }
}

void ocean (const Ctx& c)
{
    const int rows = 8 + (int) (16 * c.in.detail);
    const float step = 1.2f / std::max (0.4f, c.in.detail);
    const float t = c.in.time;
    std::vector<Vec3> line;
    for (int zi = 0; zi < rows; ++zi)
    {
        if (! c.keep (zi)) continue;
        const float z = -44.0f + (float) zi * (46.0f / (float) rows);
        line.clear();
        for (float x = -30.0f; x <= 30.0f; x += step)
        {
            float h = 0.5f * std::sin (x * 0.33f + t * 1.1f) + 0.35f * std::sin (z * 0.45f - t * 0.8f + x * 0.1f);
            h *= 0.6f + 0.8f * c.in.energy;
            for (const auto& r : c.in.ripples)
            {
                const float age = t - r.born;
                if (age < 0.0f || age > 6.0f) continue;
                const float d = std::hypot (x - r.x, z - r.z);
                h += r.strength * 0.9f * std::sin (d * 1.8f - age * 6.0f) * std::exp (-d * 0.22f) * std::exp (-age * 0.7f);
            }
            line.push_back (c.tr ({ x, -6.0f + h, z }));
        }
        const float near = (float) zi / (float) rows;
        c.rc.polyline3D (line.data(), (int) line.size(), c.pal.at (0.5f + near * 0.2f), 1.0f,
                         (0.04f + 0.2f * near) * c.k() * (0.6f + 0.6f * c.in.energy), (0.04f + 0.2f * near) * c.k() * (0.6f + 0.6f * c.in.energy));
    }
    c.rc.glow3D (c.tr ({ 0, -4.0f, -46.0f }), 26.0f, c.pal.at (0.45f), 0.16f * c.k(), 2000.0f);
}

void celestial (const Ctx& c)
{
    stars (c, (int) (460 * c.in.detail) + 80, 80.0f, 0.6f);
    Vec3 pcStar[12];
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = (float) ((pc * 7) % 12) / 12.0f * kTau;
        pcStar[pc] = { std::cos (a) * 30.0f, 10.0f + 6.0f * std::sin (a * 3.0f), std::sin (a) * 30.0f - 20.0f };
        const float act = c.in.pcActivity[(size_t) pc];
        c.rc.glow3D (c.tr (pcStar[pc]), 1.2f + 2.0f * act, c.pal.pitchClass (pc), (0.25f + 0.7f * act) * c.k());
    }
    for (int pc = 0; pc < 12; ++pc)
    {
        const int nx = (pc + 7) % 12;
        const float a = std::min (c.in.pcActivity[(size_t) pc], c.in.pcActivity[(size_t) nx]);
        if (a > 0.03f) c.rc.line3D (c.tr (pcStar[pc]), c.tr (pcStar[nx]), c.pal.pitchClass (pc), 1.0f, a * 0.8f * c.k(), true);
    }
    const Vec3 centre { 0, -3.0f, -22.0f };
    c.rc.glow3D (c.tr (centre), 4.0f + 2.0f * c.in.energy, c.pal.at (0.15f), 0.4f * c.k());
    for (int k = 0; k < 4; ++k)
    {
        const float r = 6.0f + (float) k * 5.0f, tilt = 0.25f + 0.12f * (float) k;
        ringXZ (c, centre, r, tilt, c.pal.at (0.5f), 0.08f * c.k(), 64);
        const float ang = c.in.time * 0.12f * (4.0f - (float) k) + c.in.beatPhase * 0.15f + (float) k;
        const Vec3 planet = centre + rotateX (Vec3 { std::cos (ang) * r, 0, std::sin (ang) * r }, tilt);
        c.rc.glow3D (c.tr (planet), 1.0f + 0.6f * c.in.energy, c.pal.at (0.2f * (float) k), 0.5f * c.k());
        c.rc.sphere (c.tr (planet), 0.35f + 0.1f * (float) k, c.pal.at (0.2f * (float) k), 0.9f * c.k(), Material::Matte, c.in.time);
    }
}

void crystal (const Ctx& c)
{
    for (int i = 0; i < 14; ++i)
    {
        if (! c.keep (i)) continue;
        const float a = (float) i / 14.0f * kTau + h01 ((float) i) * 0.4f;
        const float r = 12.0f + 6.0f * h01 ((float) i * 2.3f);
        const Vec3 p { std::cos (a) * r, -2.0f + 9.0f * (h01 ((float) i * 4.1f) - 0.4f), std::sin (a) * r - 6.0f };
        const int pc = i % 12;
        const float act = c.in.pcActivity[(size_t) pc];
        const float size = (1.4f + 1.8f * h01 ((float) i * 5.9f)) * (1.0f + 0.25f * c.in.chordPulse);
        MeshStyle ms;
        ms.colour = c.pal.pitchClass (pc).withMultipliedBrightness (0.5f + 0.8f * act + 0.3f * c.in.chordPulse);
        ms.fillAlpha = (0.35f + 0.4f * act) * c.k();
        ms.edgeAlpha = (0.3f + 0.6f * act) * c.k();
        ms.material = Material::Glass;
        ms.time = c.in.time;
        c.rc.mesh (meshFor (Shape::Crystal, 6), c.tr (p), { size, size * 1.6f, size },
                   { 0, c.in.time * 0.18f * (h01 ((float) i * 6.6f) - 0.5f) + (float) i + c.in.chordPulse * 0.6f, 0.15f * std::sin ((float) i) }, ms);
        if (act > 0.05f) c.rc.glow3D (c.tr (p), size * 2.5f, ms.colour, 0.25f * act * c.k());
    }
}

void voidScene (const Ctx& c)
{
    stars (c, (int) (70 * c.in.detail) + 30, 70.0f, 0.35f);
    const auto col = c.pal.at (0.5f).interpolatedWith (juce::Colours::white, 0.6f);
    c.rc.line3D (c.tr ({ -70, -3, -32 }), c.tr ({ 70, -3, -32 }), col, 1.0f, (0.1f + 0.08f * c.in.energy) * c.k(), true);
    for (int k = 0; k < 3; ++k)
    {
        MeshStyle ms; ms.colour = col; ms.fillAlpha = 0; ms.edgeAlpha = (0.05f + 0.06f * c.in.energy) * c.k(); ms.edgeWidth = 0.8f;
        const float r = 6.0f + 4.0f * (float) k;
        c.rc.mesh (meshFor (Shape::Ring, 12), c.tr ({ 0, 1.0f, -22.0f }), { r * 2, r * 2, r * 2 }, { 0.3f + c.in.time * 0.01f * (float) (k + 1), c.in.time * 0.02f, 0 }, ms);
    }
}

void bloom (const Ctx& c)
{
    const Vec3 centre { 0, -1.0f, -14.0f };
    const float grow = 0.35f + 0.65f * c.in.energy;
    for (int layer = 0; layer < 3; ++layer)
    {
        const int petals = 8 + layer * 4;
        const float R = (3.0f + 4.0f * (float) layer) * grow * (1.0f + 0.04f * std::sin (c.in.time * 0.8f + (float) layer));
        for (int i = 0; i < petals; ++i)
        {
            if (! c.keep (layer * 20 + i)) continue;
            const float a = (float) i / (float) petals * kTau + c.in.time * 0.03f * (layer % 2 ? -1.0f : 1.0f) + (float) layer * 0.3f;
            const float w = 0.45f * R / (float) petals * 4.0f;
            const Vec3 tip = centre + Vec3 { std::cos (a) * R, std::sin (a) * R, 0 };
            const Vec3 left = centre + Vec3 { std::cos (a - 0.25f) * R * 0.5f, std::sin (a - 0.25f) * R * 0.5f, w };
            const Vec3 right = centre + Vec3 { std::cos (a + 0.25f) * R * 0.5f, std::sin (a + 0.25f) * R * 0.5f, -w };
            const auto p0 = c.rc.project (c.tr (centre)), p1 = c.rc.project (c.tr (left)), p2 = c.rc.project (c.tr (tip)), p3 = c.rc.project (c.tr (right));
            if (! (p0.visible && p1.visible && p2.visible && p3.visible)) continue;
            juce::Path petal;
            petal.startNewSubPath (p0.x, p0.y);
            petal.quadraticTo (p1.x, p1.y, p2.x, p2.y);
            petal.quadraticTo (p3.x, p3.y, p0.x, p0.y);
            const auto col = c.pal.at ((float) layer * 0.2f + (float) i * 0.015f + 0.05f);
            c.rc.g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, (0.05f + 0.07f * c.in.energy) * c.k() * c.rc.alpha)));
            c.rc.g.fillPath (petal);
            c.rc.g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, 0.22f * c.k() * c.rc.alpha)));
            c.rc.g.strokePath (petal, juce::PathStrokeType (0.9f));
        }
    }
    c.rc.glow3D (c.tr (centre), 3.0f + 3.0f * c.in.energy, c.pal.at (0.1f), 0.4f * c.k());
    const int motes = 60 + (int) (100 * c.in.detail);
    for (int i = 0; i < motes; ++i)
    {
        if (! c.keep (i + 100)) continue;
        const float y = -9.0f + std::fmod (c.in.time * (0.35f + 0.6f * h01 ((float) i * 3.3f)) + h01 ((float) i * 4.4f) * 20.0f, 20.0f);
        const Vec3 p { (h01 ((float) i * 1.1f) - 0.5f) * 30.0f + std::sin (c.in.time * 0.3f + (float) i) * 0.6f, y, -4.0f - h01 ((float) i * 2.2f) * 22.0f };
        c.rc.glow3D (c.tr (p), 0.35f, c.pal.at (h01 ((float) i)), (0.15f + 0.25f * c.in.energy) * c.k() * std::min (1.0f, (11.0f - std::abs (y - 1.0f)) / 4.0f));
    }
}

void fractalBranch (const Ctx& c, const Vec3& p, const Vec3& dir, float len, int depth, int maxDepth, float spread, int& budget)
{
    if (budget-- <= 0) return;
    const Vec3 end = p + dir * len;
    const float d = (float) (maxDepth - depth) / (float) maxDepth;
    c.rc.line3D (c.tr (p), c.tr (end), c.pal.at (0.15f + d * 0.5f), 0.6f + (float) depth * 0.35f, (0.08f + 0.25f * d) * c.k());
    if (depth <= 0)
    {
        c.rc.glow3D (c.tr (end), 0.4f, c.pal.at (0.7f), (0.15f + 0.45f * c.in.energy) * c.k());
        return;
    }
    const int kids = c.in.polyphony > 0.5f ? 3 : 2;
    for (int k = 0; k < kids; ++k)
    {
        const float side = kids == 2 ? (k == 0 ? -1.0f : 1.0f) : (float) (k - 1);
        Vec3 nd = rotateZ (dir, side * spread);
        nd = rotateY (nd, 2.39996f * (float) (maxDepth - depth + k) + c.in.time * 0.05f);
        fractalBranch (c, end, nd.normalised(), len * (0.7f + 0.06f * c.in.density), depth - 1, maxDepth, spread, budget);
    }
}

void fractal (const Ctx& c)
{
    const int maxDepth = 5 + (int) std::lround (3.0f * c.in.detail);
    const float spread = 0.32f + 0.035f * (float) std::min (12, std::abs (c.in.lastInterval)) + 0.04f * (float) (std::max (0, c.in.chordType) % 5)
                       + 0.06f * std::sin (c.in.time * 0.2f);
    int budget = 900;
    fractalBranch (c, { 0, -8.0f, -12.0f }, { 0, 1, 0 }, 4.2f + 1.2f * c.in.energy, maxDepth, maxDepth, spread, budget);
}

void lattice (const Ctx& c)
{
    const int nx = c.in.detail > 0.5f ? 7 : 5, ny = 5, nz = c.in.detail > 0.5f ? 7 : 5;
    const float sp = 4.0f;
    const auto pos = [&] (int x, int y, int z) { return Vec3 { ((float) x - (float) (nx - 1) * 0.5f) * sp, ((float) y - 2.0f) * sp, ((float) z - (float) (nz - 1) * 0.5f) * sp - 8.0f }; };
    const auto bright = [&] (int x, int y, int z)
    {
        const int pc = (x * 7 + y * 4 + z * 3) % 12;
        const float wave = std::exp (-std::abs ((float) z - c.in.beatPhase * (float) nz) * 1.2f) * 0.25f * (0.4f + c.in.energy);
        return 0.05f + 0.85f * c.in.pcActivity[(size_t) pc] + wave + 0.15f * c.in.chordPulse;
    };
    const auto col = c.pal.at (0.55f);
    for (int x = 0; x < nx; ++x)
        for (int y = 0; y < ny; ++y)
            for (int z = 0; z < nz; ++z)
            {
                const int idx = (x * ny + y) * nz + z;
                if (! c.keep (idx)) continue;
                const float b = bright (x, y, z);
                const Vec3 p = c.tr (pos (x, y, z));
                if (x + 1 < nx) c.rc.line3D (p, c.tr (pos (x + 1, y, z)), col, 0.8f, (0.03f + 0.18f * std::min (b, bright (x + 1, y, z))) * c.k());
                if (y + 1 < ny) c.rc.line3D (p, c.tr (pos (x, y + 1, z)), col, 0.8f, (0.03f + 0.18f * std::min (b, bright (x, y + 1, z))) * c.k());
                if (z + 1 < nz) c.rc.line3D (p, c.tr (pos (x, y, z + 1)), col, 0.8f, (0.03f + 0.18f * std::min (b, bright (x, y, z + 1))) * c.k());
                const int pc = (x * 7 + y * 4 + z * 3) % 12;
                c.rc.glow3D (p, 0.5f + 0.9f * b, c.pal.pitchClass (pc), std::min (1.0f, b) * 0.55f * c.k());
            }
}
} // namespace

void drawScene (int scene, RenderContext& rc, const SceneInputs& in, const SceneDrawParams& dp)
{
    if (in.pal == nullptr || rc.alpha <= 0.003f) return;
    const Ctx c { rc, in, dp, *in.pal };
    switch (scene)
    {
        case 0: nebula (c); break;
        case 1: cathedral (c); break;
        case 2: aurora (c); break;
        case 3: ocean (c); break;
        case 4: celestial (c); break;
        case 5: crystal (c); break;
        case 6: voidScene (c); break;
        case 7: bloom (c); break;
        case 8: fractal (c); break;
        default: lattice (c); break;
    }
}
} // namespace eidolon
