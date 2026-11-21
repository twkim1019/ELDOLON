// EIDOLON — Mode I: Piano Universe. Every note is a spatial object with its own lifecycle:
// spawn -> velocity-driven expansion -> duration-driven persistence -> Note Off response -> decay -> dissolution.
// Pitch -> height (or radial / spiral), time -> horizontal/depth axis, channel -> depth + identity,
// pitch bend -> trajectory deformation, trails show melodic trajectories.
#include "VisualWorld.h"
#include "../Rendering/Geometry.h"
#include "../UI/Theme.h"
#include <algorithm>

namespace eidolon
{
namespace
{
constexpr float kPitchUnit = 0.16f;
float timeSpeed (const FrameContext& f) { return f.p.get (P::PianoTimeSpeed) * 2.2f; }
float releaseFor (const VisualNote& n, const FrameContext& f)
{
    float r = f.p.get (P::PianoRelease);
    if (f.p.geti (P::PianoMapDur) == 1) r *= 1.0f + (float) std::min (4.0, n.heldUntil (f.now) - n.onTime) * 0.5f;
    return r;
}
} // namespace

Vec3 PianoUniverseLayer::notePosition (const VisualNote& n, double t, const FrameContext& f, float bend)
{
    const float s = f.layerScale[0];
    const float x = (float) (t - f.now) * timeSpeed (f);
    const float pitchF = (float) n.pitch + bend * 2.0f * f.p.get (P::PianoBend);
    const float spread = kPitchUnit * f.p.get (P::PianoPitchSpread);
    const int mapChan = f.p.geti (P::PianoMapChan);
    const float chanZ = (mapChan == 0 || mapChan == 2) ? ((float) n.channel - 7.5f) * 0.55f * f.p.get (P::PianoChanSpread) : 0.0f;
    Vec3 v;
    switch (f.p.geti (P::PianoMapPitch))
    {
        case 1:   // radial tunnel around the time axis
        {
            const float a = (pitchF - 21.0f) / 88.0f * kTau * 1.0f - kPi * 0.5f;
            const float r = 3.2f * f.p.get (P::PianoPitchSpread) + chanZ * 0.3f;
            v = { x, r * std::sin (a), r * std::cos (a) };
            break;
        }
        case 2:   // spiral: pitch class sets the angle, pitch the height
        {
            const float a = (float) (((int) n.pitch * 7) % 12) / 12.0f * kTau + (pitchF - (float) n.pitch) * 0.5f;
            v = { x, (pitchF - 60.0f) * spread * 0.75f + std::cos (a) * 0.8f, std::sin (a) * 2.6f + chanZ };
            break;
        }
        default:
            v = { x, (pitchF - 60.0f) * spread, chanZ };
            break;
    }
    return (v + n.offset) * s;
}

void PianoUniverseLayer::update (const FrameContext& f)
{
    const float dt = f.dt;
    const float attraction = f.p.get (P::PianoAttraction);
    const float cluster = f.p.get (P::PianoCluster);
    const bool collide = f.p.getb (P::PianoCollide);
    const float drift = f.p.get (P::MotionDrift);
    const float rotSpeed = f.p.get (P::PianoRotSpeed);
    const float density = f.p.get (P::TrailDensity);
    const bool particlesOn = f.p.getb (P::PartOn);
    const float burst = f.p.get (P::PartBurst) * f.particleScale;
    const float sizeBase = 0.32f * f.p.get (P::PianoSize);

    order.clear();
    for (auto& n : f.world.notes) if (n.alive) order.push_back (&n);

    // physics between sounding notes (bounded: at most 96 participants)
    int participants = 0;
    for (auto* n : order)
    {
        if (! n->sounding() || participants++ > 96) continue;
        Vec3 force { -n->offset.x * 0.8f, 0.0f, -n->offset.z * 0.35f };   // spring back
        for (auto* m : order)
        {
            if (m == n || ! m->sounding()) continue;
            const int iv = std::abs ((int) n->pitch - (int) m->pitch) % 12;
            const bool consonant = iv == 3 || iv == 4 || iv == 5 || iv == 7 || iv == 8 || iv == 9;
            const float dz = m->offset.z - n->offset.z + (((float) m->channel - (float) n->channel) * 0.55f * f.p.get (P::PianoChanSpread));
            if (consonant && attraction > 0) force.z += dz * attraction * 0.9f;
            if (cluster > 0 && m->chordGroup == n->chordGroup) force.z += dz * cluster * 1.2f;
            if (collide)
            {
                const Vec3 d = n->pos - m->pos;
                const float dist = d.length(), minD = sizeBase * 1.1f;
                if (dist < minD && dist > 1e-4f) force += Vec3 { 0.0f, 0.0f, (d.z >= 0 ? 1.0f : -1.0f) } * ((minD - dist) * 18.0f);
            }
        }
        if (drift > 0) force += Vec3 { noise1 (f.time * 0.3f + n->seed * 40.0f) * 0.2f, 0.0f, noise1 (f.time * 0.25f + n->seed * 13.0f) } * (drift * 0.8f);
        n->physVel += force * dt;
        n->physVel *= std::exp (-dt * 3.0f);
        n->offset += n->physVel * dt;
        n->offset.z = clampv (n->offset.z, -5.0f, 5.0f);
        n->offset.x = clampv (n->offset.x, -1.5f, 1.5f);
    }

    const float interval = 0.022f / std::max (0.1f, density) / std::max (0.3f, f.motionQuality);
    for (auto* n : order)
    {
        if (n->sounding()) n->bend = f.tracker.pitchBend (n->channel);
        const double head = n->heldUntil (f.now);
        const float velK = 0.4f + (float) n->velocity / 127.0f;
        n->rot += n->spin * (rotSpeed * velK * dt);
        n->pos = notePosition (*n, head, f, n->bend);

        if (n->sounding() && (n->lastSample < 0 || f.now - n->lastSample >= interval))
        {
            const Vec3 raw = n->pos * (1.0f / std::max (0.01f, f.layerScale[0]));
            if (n->trailCount >= (int) n->trail.size())
            {
                // compact: keep every other sample so long notes still fit the fixed ring
                std::array<VisualNote::Sample, 64> tmp {};
                int k = 0;
                for (int i = 0; i < n->trailCount; i += 2) tmp[(size_t) k++] = n->sample (i);
                n->trail = tmp; n->trailCount = k; n->trailHead = k % (int) n->trail.size();
            }
            n->addSample (head, raw.y, raw.z);
        }

        if (particlesOn)
        {
            if (n->trailCount == 1 && ! n->burstDone && burst > 0.01f)
            {
                const int count = (int) (burst * 12.0f * ((float) n->velocity / 127.0f) + 1.0f);
                f.world.particles.burst (n->pos, count, 1.6f * f.p.get (P::PartSpeed), f.p.get (P::PartLife) * 0.6f,
                                         0.05f * f.p.get (P::PartSize), juce::Colour (n->colour), f.rng);
                n->burstDone = true;
            }
            if (! n->sounding() && f.p.getb (P::PianoParticles))
            {
                const float life = n->life (f.now, releaseFor (*n, f));
                if (life > 0.05f && f.rng.nextFloat() < 0.5f * f.particleScale * f.p.get (P::PartDensity))
                {
                    const Vec3 jitter { f.rng.bipolar(), f.rng.bipolar(), f.rng.bipolar() };
                    f.world.particles.emit (n->pos + jitter * (sizeBase * 0.4f), jitter * (0.6f * f.p.get (P::PartSpeed)) + Vec3 { -0.4f, 0.2f, 0 },
                                            f.p.get (P::PartLife) * (0.4f + 0.6f * life), 0.045f * f.p.get (P::PartSize), juce::Colour (n->colour));
                }
            }
        }
    }
}

void PianoUniverseLayer::render (RenderContext& rc, const FrameContext& f)
{
    const float s = f.layerScale[0];
    const float spread = kPitchUnit * f.p.get (P::PianoPitchSpread);
    const float speed = timeSpeed (f);
    const auto& pal = f.pal;
    const bool vertical = f.p.geti (P::PianoMapPitch) == 0;

    // ---------------------------------------------------------- spatial frame: playhead + floor grid
    if (vertical)
    {
        const float yLo = (21.0f - 60.0f) * spread * s, yHi = (108.0f - 60.0f) * spread * s;
        const auto frameCol = pal.at (0.55f).interpolatedWith (juce::Colours::white, 0.5f);
        rc.line3D ({ 0, yLo, 0 }, { 0, yHi, 0 }, frameCol, 1.0f, 0.16f, true);
        const float floorY = yLo - 0.8f * s;
        for (int i = -4; i <= 4; ++i)
        {
            const float z = (float) i * 1.5f * s;
            rc.line3D ({ -40.0f * s, floorY, z }, { 6.0f * s, floorY, z }, frameCol, 0.8f, 0.05f);
        }
        for (int i = -16; i <= 2; ++i)
        {
            const float x = (float) i * 2.5f * s;
            rc.line3D ({ x, floorY, -6.0f * s }, { x, floorY, 6.0f * s }, frameCol, 0.8f, 0.04f);
        }
        if (rc.detail > 0.45f)
            for (int oct = 1; oct <= 7; ++oct)
            {
                const float y = ((float) (oct * 12 + 12) - 60.0f) * spread * s;
                rc.line3D ({ -0.15f * s, y, 0 }, { 0.15f * s, y, 0 }, frameCol, 1.0f, 0.3f);
                rc.text3D ({ 0.55f * s, y, 0 }, "C" + juce::String (oct), ui::colours::text3, 0.7f, 10.0f);
            }
    }

    // ---------------------------------------------------------- notes, far to near
    order.clear();
    for (auto& n : f.world.notes) if (n.alive) order.push_back (&n);
    std::sort (order.begin(), order.end(), [&] (const VisualNote* a, const VisualNote* b)
    {
        return dot (a->pos - rc.cam.eye(), rc.cam.forward()) > dot (b->pos - rc.cam.eye(), rc.cam.forward());
    });

    const int geometry = f.p.geti (P::PianoGeometry);
    const int mapVel = f.p.geti (P::PianoMapVel);
    const int mapDur = f.p.geti (P::PianoMapDur);
    const int mapChan = f.p.geti (P::PianoMapChan);
    const float sizeBase = 0.32f * f.p.get (P::PianoSize) * s;
    const float velSize = f.p.get (P::PianoVelSize);
    const float glowP = f.p.get (P::PianoGlow);
    const float emission = f.p.get (P::PianoEmission);
    const float spawnP = f.p.get (P::PianoSpawn);
    const float distortion = f.p.get (P::PianoDistortion);
    const int detail = std::max (3, (int) std::lround ((float) f.p.geti (P::PianoComplexity) * (0.5f + 0.5f * rc.detail)));
    const auto material = (Material) f.p.geti (P::LookMaterial);
    const bool trailOn = f.p.getb (P::TrailOn);
    const int trailLen = std::max (2, (int) ((float) f.p.geti (P::TrailLength) * f.trailScale));
    const float trailOpacity = f.p.get (P::TrailOpacity);
    const float persistence = f.p.get (P::TrailPersist);
    const float trailWidth = f.p.get (P::TrailWidth);
    const float trailDistort = f.p.get (P::TrailDistort);
    const float trailDrift = f.p.get (P::TrailSpeed);
    const float trailDecay = f.p.get (P::TrailDecay);
    const bool smoothTrail = f.p.geti (P::TrailInterp) == 1;
    const float curvature = f.p.get (P::TrailCurve);
    const float energy = f.energy;

    for (auto* n : order)
    {
        const float release = releaseFor (*n, f);
        const float life = n->life (f.now, release);
        if (life <= 0.0f) continue;
        const float age = (float) (f.now - n->onTime);
        const float velN = (float) n->velocity / 127.0f;
        const auto col = juce::Colour (n->colour);
        float size = sizeBase, bright = 0.85f, glowK = 1.0f;
        switch (mapVel)
        {
            case 0: size *= 0.45f + velN * velSize * 0.9f; break;
            case 1: bright = 0.35f + 0.65f * velN * std::min (1.5f, velSize); break;
            case 2: size *= 0.7f + velN * 0.3f; glowK = 0.5f + velN * velSize * 1.2f; break;
            default: size *= 0.45f + velN * velSize * 0.9f; bright = 0.4f + 0.6f * velN; break;
        }
        const float held = (float) (n->heldUntil (f.now) - n->onTime);
        if (mapDur == 2) size *= 1.0f + std::min (3.0f, held) * 0.25f;
        size *= 1.0f + spawnP * 0.9f * std::exp (-age * 7.0f) * std::sin (std::min (age * 14.0f, kPi));
        size *= 0.55f + 0.45f * life;
        size *= 0.85f + 0.15f * energy;
        const float alpha = std::pow (life, 1.3f) * std::min (1.0f, 0.4f + 0.6f * energy);
        n->drawScale = size;

        // ------------------------------------------------ trail / body (melodic trajectory, bend history)
        if ((trailOn || mapDur == 0 || geometry == 6) && n->trailCount >= 1)
        {
            const int count = std::min (n->trailCount, trailOn ? trailLen : (int) n->trail.size());
            pts.clear(); widths.clear();
            const int start = n->trailCount - count;
            for (int i = start; i < n->trailCount; ++i)
            {
                const auto& smp = n->sample (i);
                const float ageS = (float) (f.now - smp.t);
                Vec3 p { (float) (smp.t - f.now) * speed, smp.y, smp.z };
                if (trailDistort > 0) p += Vec3 { 0, noise1 (ageS * 1.7f + n->seed * 31.0f), noise1 (ageS * 1.3f + n->seed * 17.0f) } * (trailDistort * std::min (1.0f, ageS) * 0.5f);
                if (trailDrift > 0) p.z += std::sin (n->seed * 20.0f) * ageS * trailDrift * 0.25f;
                pts.push_back (p * s);
            }
            pts.push_back (n->pos);
            if (smoothTrail && pts.size() >= 3 && curvature > 0.01f)
                for (int pass = 0; pass < 2; ++pass)
                    for (size_t i = 1; i + 1 < pts.size(); ++i)
                        pts[i] = lerpv (pts[i], (pts[i - 1] + pts[i + 1]) * 0.5f, curvature * 0.5f);
            const int np = (int) pts.size();
            for (int i = 0; i < np; ++i)
            {
                const float t = np > 1 ? (float) i / (float) (np - 1) : 1.0f;
                widths.push_back (size * 0.42f * trailWidth * (0.25f + 0.75f * std::pow (t, 0.6f + trailDecay)));
            }
            const float aOld = trailOpacity * alpha * (0.02f + 0.5f * persistence * persistence);
            const float aNew = trailOpacity * alpha;
            if (np >= 2)
            {
                if (mapDur == 0 || geometry == 6)
                {
                    rc.ribbon3D (pts.data(), widths.data(), np, col.withMultipliedBrightness (0.8f * bright), aOld * 0.5f, aNew * 0.55f);
                    rc.polyline3D (pts.data(), np, col.brighter (0.4f), 1.2f, aOld, aNew * 0.95f);
                }
                else
                {
                    rc.polyline3D (pts.data(), np, col, 1.6f * trailWidth, aOld, aNew * 0.8f, 1.6f);
                }
            }
        }

        // ------------------------------------------------ halo + birth flash
        rc.glow3D (n->pos, size * (1.6f + 2.6f * rc.bloom) * glowK, col, 0.32f * alpha * glowP * bright * emission);
        if (age < 0.6f && spawnP > 0.01f)
        {
            const float k = age / 0.6f;
            const auto pr = rc.project (n->pos);
            if (pr.visible)
            {
                const float r = size * pr.scale * (1.0f + 4.0f * easeOutCubic (k));
                rc.g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, (1.0f - k) * 0.45f * spawnP * rc.alpha)));
                rc.g.drawEllipse (pr.x - r, pr.y - r * 0.92f, 2 * r, 2 * r * 0.92f, 1.2f);
            }
        }

        // ------------------------------------------------ the object itself
        const int shape = mapChan == 3 ? (n->channel % 11) : geometry;
        MeshStyle ms;
        ms.colour = col.withMultipliedBrightness (bright);
        ms.fillAlpha = alpha * 0.85f;
        ms.edgeAlpha = alpha * 0.9f;
        ms.edgeWidth = 1.0f;
        ms.material = material;
        ms.distortion = distortion;
        ms.time = f.time;
        ms.seed = n->seed;
        ms.emission = emission;
        switch ((Shape) shape)
        {
            case Shape::Sphere:
                rc.sphere (n->pos, size * 0.5f, ms.colour, alpha, material, f.time);
                break;
            case Shape::Particle:
            {
                const int dots = 5 + (int) (6.0f * rc.detail);
                for (int i = 0; i < dots; ++i)
                {
                    const float a = n->rot.y + (float) i / (float) dots * kTau;
                    const float rr = size * (0.35f + 0.25f * std::sin (f.time * 2.0f + (float) i * 1.7f + n->seed * 9.0f));
                    rc.glow3D (n->pos + Vec3 { std::cos (a) * rr, std::sin (a * 1.3f) * rr * 0.8f, std::sin (a) * rr }, size * 0.35f, ms.colour, alpha * 0.8f);
                }
                rc.glow3D (n->pos, size * 0.4f, ms.colour.brighter (0.5f), alpha);
                break;
            }
            case Shape::Ribbon:
                rc.sphere (n->pos, size * 0.22f, ms.colour.brighter (0.3f), alpha, Material::Luminous, f.time);
                break;
            case Shape::Cylinder:
            {
                const float len = std::max (size, held * speed * s);
                const Vec3 c = n->pos - Vec3 { len * 0.5f - size * 0.5f, 0, 0 };
                rc.mesh (meshFor (Shape::Cylinder, detail), c, { len, size * 0.55f, size * 0.55f }, { n->rot.x, 0, 0 }, ms);
                break;
            }
            default:
                rc.mesh (meshFor ((Shape) shape, detail), n->pos, { size, size, size }, n->rot, ms);
                break;
        }
    }
}
} // namespace eidolon
