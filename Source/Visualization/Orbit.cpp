// EIDOLON — Mode VI: Orbit. The twelve pitch classes around a circle, octave rings, notes as orbiting bodies
// whose radius / angle follow the selected orbit mode, with arcs, chord connections and pitch-class glow.
#include "VisualWorld.h"
#include "../Intelligence/ChordDetector.h"
#include "../UI/Theme.h"

namespace eidolon
{
namespace
{
struct OrbitPos { Vec3 p; float angle; float radius; };

OrbitPos orbitPosition (const VisualNote& n, const FrameContext& f, float spin, double atTime)
{
    const float R = f.p.get (P::OrbRadius) * 2.2f * f.layerScale[5];
    const bool fifths = f.p.getb (P::OrbFifths);
    const int pc = n.pitch % 12;
    const float pcAngle = (float) (fifths ? (pc * 7) % 12 : pc) / 12.0f * kTau;
    const int octave = n.pitch / 12 - 1;
    const float age = (float) (atTime - n.onTime);
    const float vel = (float) n.velocity / 127.0f;
    float angle = pcAngle, radius = R * (0.55f + 0.25f * (float) clampv (octave, 0, 8)), y = 0.0f;
    float omega = 0.12f;
    switch (f.p.geti (P::OrbMode))
    {
        case 0: omega = 0.05f + 0.02f * (float) (8 - clampv (octave, 0, 8)); break;                              // pitch
        case 1: radius = R * 1.4f; y = ((float) octave - 4.0f) * 0.35f * f.layerScale[5]; omega = 0.08f; break; // pitch class
        case 2: angle = (float) clampv (octave, 0, 8) / 9.0f * kTau; radius = R * (0.5f + (float) pc / 12.0f * 1.4f); break;
        case 3:
        {
            const bool inChord = f.a.chord.valid && (f.a.chord.pitchClasses & (1u << pc)) && n.sounding();
            radius = inChord ? R * 0.7f : R * 1.9f;
            omega = inChord ? 0.45f : 0.06f;
            break;
        }
        case 4: radius = R * (0.4f + 1.6f * vel); omega = 0.05f + 0.3f * vel; break;
        case 5: radius = R * (0.4f + std::min (2.0f, (float) (n.heldUntil (atTime) - n.onTime)) * 0.8f); break;
        case 6: radius = R * (0.5f + 0.32f * (float) std::max (0, n.voice)); omega = 0.1f + 0.05f * (float) std::max (0, n.voice); break;
        default:
        {
            int consonance = 2;
            if (f.a.chord.valid)
            {
                const int iv = ((pc - f.a.chord.root) % 12 + 12) % 12;
                consonance = (iv == 0) ? 0 : (iv == 7 || iv == 5) ? 1 : (iv == 3 || iv == 4 || iv == 8 || iv == 9) ? 2 : 4;
            }
            radius = R * (0.45f + 0.38f * (float) consonance);
            break;
        }
    }
    angle += spin + age * omega * (0.6f + vel) * f.p.get (P::MotionSpeed);
    return { Vec3 { std::cos (angle) * radius, y + vel * 0.25f * f.layerScale[5], std::sin (angle) * radius }, angle, radius };
}
} // namespace

void OrbitLayer::update (const FrameContext& f)
{
    spin = f.p.get (P::OrbRotation) * kPi / 180.0f + (float) f.time * f.p.get (P::OrbRotSpeed);
    std::array<float, 12> target {};
    f.tracker.forEachActive ([&] (int, int note, const NoteSlot& s) { target[(size_t) (note % 12)] = std::max (target[(size_t) (note % 12)], 0.4f + (float) s.velocity / 210.0f); });
    for (int i = 0; i < 12; ++i) pcGlow[(size_t) i] += (target[(size_t) i] - pcGlow[(size_t) i]) * smoothCoef (f.dt, target[(size_t) i] > pcGlow[(size_t) i] ? 0.03f : 0.5f);
}

void OrbitLayer::render (RenderContext& rc, const FrameContext& f)
{
    const float s = f.layerScale[5];
    const float R = f.p.get (P::OrbRadius) * 2.2f * s;
    const bool fifths = f.p.getb (P::OrbFifths);
    const auto frame = f.pal.at (0.5f).interpolatedWith (juce::Colours::white, 0.55f);

    // octave rings
    if (f.p.getb (P::OrbRings))
        for (int o = 0; o < 9; ++o)
        {
            const float r = R * (0.55f + 0.25f * (float) o);
            Vec3 prev { r, 0, 0 };
            const int segs = 48 + (int) (32.0f * rc.detail);
            for (int i = 1; i <= segs; ++i)
            {
                const float a = (float) i / (float) segs * kTau;
                const Vec3 q { std::cos (a) * r, 0, std::sin (a) * r };
                rc.line3D (prev, q, frame, 0.8f, o == 4 ? 0.12f : 0.06f);
                prev = q;
            }
        }

    // pitch-class spokes + labels
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = (float) (fifths ? (pc * 7) % 12 : pc) / 12.0f * kTau + spin;
        const Vec3 outer { std::cos (a) * R * 2.75f, 0, std::sin (a) * R * 2.75f };
        const float glow = pcGlow[(size_t) pc];
        const auto c = f.pal.pitchClass (pc);
        rc.line3D (Vec3 { std::cos (a) * R * 0.45f, 0, std::sin (a) * R * 0.45f }, outer, c, 0.8f, 0.07f + 0.35f * glow);
        if (glow > 0.02f) rc.glow3D (outer, 0.5f * s + glow * 0.4f * s, c, 0.6f * glow);
        rc.text3D (outer * 1.09f, ChordDetector::pitchName (pc, false), glow > 0.05f ? c.interpolatedWith (juce::Colours::white, 0.5f) : ui::colours::text3,
                   0.55f + 0.45f * glow, 11.0f);
    }

    // notes
    const float nodeSize = f.p.get (P::OrbNodeSize) * 0.22f * s;
    const int geo = f.p.geti (P::OrbGeometry);
    const bool trails = f.p.getb (P::OrbTrail);
    std::vector<Vec3> sounding;
    for (const auto& n : f.world.notes)
    {
        if (! n.alive) continue;
        const float life = n.life (f.now, std::max (0.6f, f.p.get (P::PianoRelease)));
        if (life <= 0.0f) continue;
        const auto op = orbitPosition (n, f, spin, f.now);
        const auto c = juce::Colour (n.colour);
        const float vel = (float) n.velocity / 127.0f;
        const float size = nodeSize * (0.6f + 0.8f * vel) * (0.5f + 0.5f * life);
        if (trails)
        {
            Vec3 pts[16];
            for (int k = 0; k < 16; ++k)
            {
                const double t = f.now - (double) k * 0.06;
                if (t < n.onTime) { pts[k] = op.p; continue; }
                pts[k] = orbitPosition (n, f, spin, t).p;
            }
            rc.polyline3D (pts, 16, c, 1.4f, 0.8f * life, 0.0f, 0.3f);
        }
        rc.glow3D (op.p, size * (3.0f + 3.0f * rc.bloom), c, 0.35f * life);
        switch (geo)
        {
            case 1:
            {
                MeshStyle ms; ms.colour = c; ms.fillAlpha = 0; ms.edgeAlpha = life; ms.edgeWidth = 1.3f;
                rc.mesh (meshFor (Shape::Ring, 6), op.p, { size * 2, size * 2, size * 2 }, { kPi * 0.5f, f.time, 0 }, ms);
                break;
            }
            case 2:
            {
                MeshStyle ms; ms.colour = c; ms.fillAlpha = life * 0.7f; ms.edgeAlpha = life; ms.material = Material::Glass;
                rc.mesh (meshFor (Shape::Crystal, 5), op.p, { size * 1.6f, size * 1.6f, size * 1.6f }, { 0, f.time + n.seed * 6, 0 }, ms);
                break;
            }
            case 3:
            {
                MeshStyle ms; ms.colour = c; ms.fillAlpha = life * 0.4f; ms.edgeAlpha = life;
                rc.mesh (meshFor (Shape::Polygon, 3 + n.pitch % 5), op.p, { size * 2, size * 2, size * 2 }, { f.time * 0.4f, n.seed * 6, 0 }, ms);
                break;
            }
            default: rc.sphere (op.p, size * 0.6f, c.brighter (0.2f), life, Material::Luminous, f.time); break;
        }
        if (n.sounding()) sounding.push_back (op.p);
    }

    // chord connections inside the circle
    if (f.p.getb (P::OrbConnect) && sounding.size() >= 2)
    {
        const auto c = f.a.chord.valid ? f.pal.pitchClass (f.a.chord.root) : frame;
        for (size_t i = 0; i < sounding.size() && i < 24; ++i)
            for (size_t j = i + 1; j < sounding.size() && j < 24; ++j)
                rc.line3D (sounding[i], sounding[j], c, 1.0f, 0.35f);
        rc.glow3D ({ 0, 0, 0 }, R * 0.5f, c, 0.15f + 0.1f * (float) sounding.size() / 8.0f);
    }
}
} // namespace eidolon
