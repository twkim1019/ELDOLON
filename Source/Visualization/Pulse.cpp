// EIDOLON — Mode IV: Pulse. Rhythmic activity as an energy system. Every pulse is randomized
// (shape, rotation, deformation, tint) so repeated hits never look mechanically identical.
#include "VisualWorld.h"
#include "../UI/Theme.h"

namespace eidolon
{
void PulseLayer::trigger (const FrameContext& f, float strength, float hue)
{
    const float var = f.p.get (P::PulseVariation);
    auto& p = pulses[(size_t) (count++ % (int) pulses.size())];
    p.born = f.now;
    p.strength = strength * f.p.get (P::PulseStrength) * (1.0f + var * 0.3f * f.rng.bipolar());
    p.hue = hue + var * 0.06f * f.rng.bipolar();
    p.seed = f.rng.nextFloat();
    p.ellipse = 1.0f + var * 0.35f * f.rng.bipolar();
    p.rot = f.rng.range (0, kTau);
    p.deform = f.rng.range (0, kTau);
    p.sides = 3 + std::min (9, f.a.polyphony);
    envelope = std::max (envelope, strength);
    lastTrigger = f.now;

    if (f.p.geti (P::PulseStyle) == 2 && f.p.getb (P::PartOn))
    {
        const auto c = f.pal.at (p.hue);
        const int n = (int) (40.0f * strength * f.particleScale * f.p.get (P::PartDensity)) + 4;
        f.world.particles.burst (Vec3 { 0, 0, 0 }, n, 3.5f * f.p.get (P::PulseExpansion) * f.p.get (P::PartSpeed) * f.layerScale[3],
                                 f.p.get (P::PartLife) * 0.7f, 0.06f * f.p.get (P::PartSize), c, f.rng, 0.6f);
    }
}

void PulseLayer::update (const FrameContext& f)
{
    const int mode = f.p.geti (P::PulseTrigger);
    auto pcHue = [] (int pc) { return (float) ((pc * 7) % 12) / 12.0f; };
    switch (mode)
    {
        case 0: case 2:
            for (const auto& c : f.changes)
                if (c.kind == NoteChange::Start && (mode == 0 || c.velocity >= f.p.geti (P::PulseVelThresh)))
                    trigger (f, 0.35f + 0.65f * (float) c.velocity / 127.0f, pcHue (c.pitch % 12));
            break;
        case 1:
            for (const auto& c : f.changes) if (c.kind == NoteChange::Release) trigger (f, 0.6f, pcHue (c.pitch % 12));
            break;
        case 3:
        {
            const float rate = f.a.rhythm.onsetRate;
            if (rate > 0.3f && f.now - lastTrigger > 1.0 / std::max (0.5, (double) rate * 0.5))
                trigger (f, saturate (0.3f + f.a.rhythm.density), (float) f.now * 0.02f);
            break;
        }
        case 4: if (f.a.rhythm.beatTick) trigger (f, f.a.rhythm.beatInBar == 0 ? 1.0f : 0.65f, f.a.rhythm.beatInBar * 0.08f); break;
        case 5: if (f.a.rhythm.barTick) trigger (f, 1.0f, (float) f.now * 0.01f); break;
        case 6:
        {
            const float level = (float) f.a.polyphony;
            if (level > polyLevel + 0.5f) trigger (f, saturate (level / 8.0f + 0.2f), f.chordRoot >= 0 ? pcHue (f.chordRoot) : 0.3f);
            polyLevel = level;
            break;
        }
        case 7:
        {
            float v = 0;
            for (int ch = 0; ch < 16; ++ch) v = std::max (v, f.tracker.cc (ch, f.p.geti (P::PulseCC)));
            if (v > lastCC + 0.15f || (v >= 0.5f && lastCC < 0.5f)) { trigger (f, 0.3f + 0.7f * v, v); lastCC = v; }
            else if (v < lastCC - 0.15f) lastCC = v;
            break;
        }
        default:
            if (f.a.chordChanged && f.a.chord.valid) trigger (f, 0.5f + 0.5f * f.a.chord.confidence, pcHue (f.a.chord.root));
            break;
    }
    envelope *= std::exp (-f.dt / std::max (0.05f, f.p.get (P::PulseDecay) * 0.6f));
    osc += f.dt * f.p.get (P::PulseFreq) * (0.5f + f.activity);
}

void PulseLayer::render (RenderContext& rc, const FrameContext& f)
{
    const float s = f.layerScale[3];
    const float decay = f.p.get (P::PulseDecay);
    const float expansion = f.p.get (P::PulseExpansion);
    const float deform = f.p.get (P::PulseDeform);
    const int style = f.p.geti (P::PulseStyle);
    const bool sync = f.p.getb (P::PulseSync) && f.a.rhythm.source != BeatSource::None;
    const float phase = sync ? f.a.rhythm.beatPhase * kTau : osc * kTau;

    const Vec3 centre { 0, (f.a.registerCenter - 60.0f) * 0.06f * s, 0 };
    const auto pc = rc.project (centre);
    if (! pc.visible) return;
    const float squash = clampv (std::abs (std::sin (rc.cam.pitch)) + 0.18f, 0.25f, 1.0f);
    const float rMax = f.p.get (P::PulseRadius) * 4.5f * s * pc.scale;
    auto& g = rc.g;

    // continuous core
    const float coreR = rMax * (0.08f + 0.12f * envelope + 0.02f * std::sin (phase));
    const auto coreCol = f.pal.at (0.1f + 0.05f * std::sin (f.time * 0.2f));
    rc.glow (pc.x, pc.y, coreR * (3.0f + 2.0f * rc.bloom), coreCol, 0.25f + 0.5f * envelope);
    if (style == 5)
    {
        const float br = rMax * (0.18f + 0.35f * envelope) * (1.0f + 0.06f * std::sin (phase));
        rc.glow (pc.x, pc.y, br * 2.2f, coreCol.brighter (0.3f), 0.5f + 0.4f * envelope);
        g.setColour (coreCol.withAlpha (juce::jlimit (0.0f, 1.0f, 0.45f * rc.alpha)));
        g.drawEllipse (pc.x - br, pc.y - br * squash, 2 * br, 2 * br * squash, 1.2f);
    }

    const int n = std::min (count, (int) pulses.size());
    for (int i = 0; i < n; ++i)
    {
        const auto& p = pulses[(size_t) i];
        const float age = (float) (f.now - p.born);
        const float life = decay * 2.0f;
        if (age < 0 || age > life) continue;
        const float k = age / life;
        const float grow = easeOutCubic (std::min (1.0f, age * expansion / std::max (0.1f, decay * 1.1f)));
        const float r = rMax * (0.12f + 0.88f * grow);
        const float a = juce::jlimit (0.0f, 1.0f, p.strength * std::pow (1.0f - k, 1.5f));
        const auto c = f.pal.at (p.hue);
        const float ex = p.ellipse, ey = squash / p.ellipse;

        auto ringPath = [&] (float radius, float lobes, float amount)
        {
            juce::Path path;
            const int segs = 48 + (int) (48.0f * rc.detail);
            for (int j = 0; j <= segs; ++j)
            {
                const float th = (float) j / (float) segs * kTau;
                const float rr = radius * (1.0f + amount * std::sin (th * lobes + p.deform + phase * 0.5f));
                const float x = std::cos (th + p.rot) * rr * ex, y = std::sin (th + p.rot) * rr;
                const float yy = y * ey;
                if (j == 0) path.startNewSubPath (pc.x + x, pc.y + yy); else path.lineTo (pc.x + x, pc.y + yy);
            }
            return path;
        };

        switch (style)
        {
            case 0: case 2: case 5:
            {
                const auto path = ringPath (r, 3.0f + std::floor (p.seed * 5.0f), deform * 0.08f);
                g.setColour (c.withAlpha (a * 0.18f * rc.alpha));
                g.strokePath (path, juce::PathStrokeType (6.0f + 10.0f * rc.bloom));
                g.setColour (c.brighter (0.3f).withAlpha (a * 0.85f * rc.alpha));
                g.strokePath (path, juce::PathStrokeType (1.3f + 1.5f * (1.0f - k)));
                break;
            }
            case 1:
                for (int w = 0; w < 3; ++w)
                {
                    const auto path = ringPath (r * (1.0f - 0.18f * (float) w), 6.0f + (float) w, deform * 0.05f);
                    g.setColour (c.withAlpha (a * (0.7f - 0.2f * (float) w) * rc.alpha));
                    g.strokePath (path, juce::PathStrokeType (1.0f));
                }
                break;
            case 3:
            {
                juce::Path poly;
                for (int j = 0; j <= p.sides; ++j)
                {
                    const float th = (float) j / (float) p.sides * kTau + p.rot + f.time * 0.3f;
                    const float rr = r * (1.0f + deform * 0.2f * std::sin (phase * 2.0f + (float) j));
                    const float x = pc.x + std::cos (th) * rr * ex, y = pc.y + std::sin (th) * rr * ey;
                    if (j == 0) poly.startNewSubPath (x, y); else poly.lineTo (x, y);
                }
                g.setColour (c.withAlpha (a * 0.08f * rc.alpha));
                g.fillPath (poly);
                g.setColour (c.withAlpha (a * 0.8f * rc.alpha));
                g.strokePath (poly, juce::PathStrokeType (1.3f));
                break;
            }
            default:
            {
                const int rays = 10 + (int) (p.seed * 14.0f);
                for (int j = 0; j < rays; ++j)
                {
                    const float th = (float) j / (float) rays * kTau + p.rot;
                    const float r0 = r * 0.3f, r1 = r * (0.7f + 0.3f * std::sin (p.deform + (float) j * 1.7f));
                    rc.line2D (pc.x + std::cos (th) * r0 * ex, pc.y + std::sin (th) * r0 * ey,
                               pc.x + std::cos (th) * r1 * ex, pc.y + std::sin (th) * r1 * ey, c, 1.2f, a * 0.8f);
                }
                break;
            }
        }
    }
}
} // namespace eidolon
