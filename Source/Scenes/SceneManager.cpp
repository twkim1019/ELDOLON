#include "SceneManager.h"

namespace eidolon
{
namespace
{
// Cinematic-mode layer mix per scene: piano, constellation, flow, pulse, spectrum, orbit
constexpr float kProfiles[kNumScenes][6] = {
    { 0.95f, 0.00f, 0.45f, 0.00f, 0, 0.00f },   // Nebula
    { 0.55f, 0.90f, 0.00f, 0.40f, 0, 0.00f },   // Cathedral
    { 0.35f, 0.00f, 1.00f, 0.00f, 0, 0.00f },   // Aurora
    { 0.70f, 0.00f, 0.00f, 0.90f, 0, 0.00f },   // Ocean
    { 0.30f, 0.60f, 0.00f, 0.00f, 0, 1.00f },   // Celestial
    { 1.00f, 0.70f, 0.00f, 0.00f, 0, 0.00f },   // Crystal
    { 0.80f, 0.00f, 0.00f, 0.00f, 0, 0.00f },   // Void
    { 0.50f, 0.00f, 0.00f, 1.00f, 0, 0.00f },   // Bloom
    { 0.40f, 1.00f, 0.00f, 0.00f, 0, 0.00f },   // Fractal
    { 0.50f, 0.90f, 0.00f, 0.00f, 0, 0.40f },   // Lattice
};
constexpr float kSceneEnergy[kNumScenes] = { 0.35f, 0.55f, 0.5f, 0.7f, 0.4f, 0.65f, 0.1f, 0.85f, 0.75f, 0.6f };
} // namespace

const char* sceneName (int index)
{
    static const char* n[] = { "Nebula", "Cathedral", "Aurora", "Ocean", "Celestial", "Crystal", "Void", "Bloom", "Fractal", "Lattice" };
    return n[clampv (index, 0, kNumScenes - 1)];
}

std::array<float, 6> SceneManager::layerWeights() const
{
    std::array<float, 6> w {};
    const float e = easeInOut (t);
    for (int i = 0; i < 6; ++i) w[(size_t) i] = lerpf (kProfiles[prev][i], kProfiles[cur][i], e);
    return w;
}

void SceneManager::startTransition (int target, int type, double now, float dur)
{
    target = clampv (target, 0, kNumScenes - 1);
    if (target == cur) return;
    prev = cur;
    cur = target;
    t = 0.0f;
    transType = type;
    duration = std::max (0.2f, dur);
    lastChange = now;
    changedFlag = true;
    particleBurstPending = type == 6;
    notesSince = 0;
}

void SceneManager::requestScene (int s) { pendingTarget = clampv (s, 0, kNumScenes - 1); }
void SceneManager::next() { pendingNext = true; }

int SceneManager::chooseNext (const FrameContext& f)
{
    switch (f.p.geti (P::SceneOrder))
    {
        case 0: return (cur + 1) % kNumScenes;
        case 1: { int s = cur; for (int i = 0; i < 8 && s == cur; ++i) s = rng.nextInt (kNumScenes); return s; }
        default:
        {
            // energy-matched: the scene whose character best fits the music right now (never the current one)
            const float e = saturate (energySmooth);
            int best = (cur + 1) % kNumScenes; float bestD = 9;
            for (int s = 0; s < kNumScenes; ++s)
            {
                if (s == cur || s == prev) continue;
                const float d = std::abs (kSceneEnergy[s] - e) + 0.08f * rng.nextFloat();
                if (d < bestD) { bestD = d; best = s; }
            }
            return best;
        }
    }
}

bool SceneManager::evaluateTrigger (const FrameContext& f)
{
    const float thr = f.p.get (P::SceneThreshold);
    const float sens = f.p.get (P::SceneSensitivity);
    const auto& a = f.a;
    float level = -1.0f;   // level-based triggers (crossing upward with hysteresis)
    bool event = false;    // event triggers
    switch (f.p.geti (P::SceneTrigger))
    {
        case 0: level = a.noteDensity / 12.0f * sens; break;
        case 1: level = (float) a.polyphony / 8.0f * sens; break;
        case 2: level = a.recentVelocity / 127.0f * sens; break;
        case 3: event = a.chordChanged && a.chord.valid && a.chord.confidence >= thr * 0.8f; break;
        case 4: level = std::abs (a.registerCenter - lastRegister) / 24.0f * sens; break;
        case 5: level = a.rhythm.onsetRate / 10.0f * sens; break;
        case 6:
        {
            float v = 0;
            for (int ch = 0; ch < 16; ++ch) v = std::max (v, f.tracker.cc (ch, f.p.geti (P::SceneCC)));
            level = v;
            break;
        }
        case 7: event = a.rhythm.beatTick && a.rhythm.beatInBar == 0; break;
        case 8: event = a.rhythm.barTick; break;
        case 9:
        {
            const int root = a.scale.valid && a.scale.confidence > 0.35f ? a.scale.root : -1;
            event = root >= 0 && lastScaleRoot >= 0 && root != lastScaleRoot;
            if (root >= 0) lastScaleRoot = root;
            break;
        }
        case 10: level = (float) notesSince / (16.0f + 240.0f * thr) ; level = level >= 1.0f ? 1.0f : 0.0f; break;
        default: return false;   // manual only
    }
    if (event) return true;
    if (level < 0.0f) return false;
    const float threshold = f.p.geti (P::SceneTrigger) == 10 ? 0.5f : thr;
    if (levelArmed && level >= threshold) { levelArmed = false; return true; }
    if (level < threshold * 0.7f) levelArmed = true;
    return false;
}

void SceneManager::update (const FrameContext& f, float advanceValue)
{
    changedFlag = false;
    for (const auto& c : f.changes)
        if (c.kind == NoteChange::Start)
        {
            ++notesSince;
            auto& r = in.ripples[(size_t) (notesSince % (int) in.ripples.size())];
            r.x = ((float) c.pitch - 64.0f) * 0.35f;
            r.z = -6.0f + ((float) c.channel - 7.5f) * 1.2f;
            r.born = f.time;
            r.strength = (float) c.velocity / 127.0f;
        }
    energySmooth += (f.activity - energySmooth) * smoothCoef (f.dt, 2.0f);

    const int ps = f.p.geti (P::SceneCurrent);
    const float dur = f.p.get (P::SceneTransTime);
    const int type = f.p.geti (P::SceneTransType);
    if (paramScene < 0) { paramScene = ps; cur = prev = ps; }
    else if (ps != paramScene) { paramScene = ps; startTransition (ps, type, f.now, dur); }

    if (pendingTarget >= 0) { startTransition (pendingTarget, type, f.now, dur); pendingTarget = -1; }
    if (pendingNext) { startTransition (chooseNext (f), type, f.now, dur); pendingNext = false; }
    if (advanceValue >= 0.5f && lastAdvance < 0.5f) startTransition (chooseNext (f), type, f.now, dur);   // mapping target
    lastAdvance = advanceValue;

    if (f.p.getb (P::SceneAuto) && t >= 1.0f && f.now - lastChange >= f.p.get (P::SceneMinHold))
        if (evaluateTrigger (f))
        {
            startTransition (chooseNext (f), type, f.now, dur);
            lastRegister = f.a.registerCenter;
        }

    if (t < 1.0f) t = std::min (1.0f, t + f.dt / duration);

    if (particleBurstPending && f.p.getb (P::PartOn))
    {
        particleBurstPending = false;
        for (int i = 0; i < 6; ++i)
            f.world.particles.burst (Vec3 { f.rng.range (-8, 8), f.rng.range (-4, 4), f.rng.range (-6, 2) }, (int) (30 * f.particleScale) + 5,
                                     3.0f, 2.5f, 0.07f, f.pal.at (f.rng.nextFloat()), f.rng);
    }

    // scene inputs
    in.time = f.time;
    in.energy = saturate (f.activity * f.p.get (P::SceneSensitivity)) * f.energy;
    in.density = saturate (f.a.noteDensity / 12.0f);
    in.polyphony = saturate ((float) f.a.polyphony / 8.0f);
    in.beatPhase = f.a.rhythm.source != BeatSource::None ? f.a.rhythm.beatPhase : (float) std::fmod (f.time * 0.5f, 1.0f);
    in.barPhase = f.a.rhythm.barPhase;
    in.beatTick = f.a.rhythm.beatTick;
    in.chordChanged = f.a.chordChanged;
    in.chordPulse = f.a.chordChanged ? 1.0f : in.chordPulse * std::exp (-f.dt * 1.2f);
    std::array<float, 12> pc {};
    f.tracker.forEachActive ([&] (int, int n, const NoteSlot& sl) { pc[(size_t) (n % 12)] = std::max (pc[(size_t) (n % 12)], (float) sl.velocity / 127.0f); });
    for (int i = 0; i < 12; ++i) in.pcActivity[(size_t) i] += (pc[(size_t) i] - in.pcActivity[(size_t) i]) * smoothCoef (f.dt, pc[(size_t) i] > in.pcActivity[(size_t) i] ? 0.05f : 0.9f);
    in.registerNorm = clampv ((f.a.registerCenter - 60.0f) / 24.0f, -1.0f, 1.0f);
    in.contour = f.a.contour;
    in.lastInterval = f.a.lastInterval;
    in.chordType = f.a.chord.valid ? f.a.chord.templateIndex : -1;
    in.chordRoot = f.a.chord.valid ? f.a.chord.root : -1;
    in.intensity = f.p.get (P::SceneIntensity);
    in.sensitivity = f.p.get (P::SceneSensitivity);
    in.detail = f.detailScale;
    in.pal = &f.pal;
}

void SceneManager::render (RenderContext& rc, const FrameContext& f)
{
    in.seed = (uint32_t) (f.p.get (P::MotionSeedUse) * 1000.0f);
    const float e = easeInOut (t);
    SceneDrawParams outP, inP;
    switch (transType)
    {
        case 1: outP.alpha = 1 - e; inP.alpha = e; outP.dissolve = e; inP.dissolve = 1 - e; break;                         // dissolve
        case 2: outP.alpha = 1 - e; inP.alpha = e; outP.scale = 1 + 0.15f * e; inP.scale = 0.85f + 0.15f * e; break;     // morph
        case 3: outP.alpha = 1 - e; inP.alpha = e; outP.scale = 1 + 2.0f * e; inP.scale = 0.3f + 0.7f * e; break;        // expansion
        case 4: outP.alpha = 1 - e; inP.alpha = e; outP.scale = 1 - 0.9f * e; inP.scale = 1.6f - 0.6f * e; break;        // collapse
        case 5: outP.alpha = 1 - e; inP.alpha = e; outP.rotation = e * kPi * 0.5f; inP.rotation = (e - 1) * kPi * 0.5f;  // transformation
                outP.scale = 1 - 0.2f * std::sin (e * kPi); inP.scale = outP.scale; break;
        default: outP.alpha = 1 - e; inP.alpha = e; break;                                                                // crossfade / particle / geometry
    }
    outP.scale *= f.layerScale[6];
    inP.scale *= f.layerScale[6];
    const float blend = f.p.get (P::SceneBlend);
    const float baseAlpha = rc.alpha;
    if (t < 1.0f || blend > 0.01f)
    {
        SceneDrawParams keep = outP;
        keep.alpha = std::max (outP.alpha, t >= 1.0f ? blend : outP.alpha);
        if (prev != cur && keep.alpha > 0.01f) { rc.alpha = baseAlpha * keep.alpha; drawScene (prev, rc, in, keep); }
    }
    rc.alpha = baseAlpha * inP.alpha;
    drawScene (cur, rc, in, inP);
    rc.alpha = baseAlpha;

    if (transType == 7 && t < 1.0f)   // geometry transition: a wireframe lattice sweeps through
    {
        const float k = std::sin (t * kPi);
        MeshStyle ms; ms.colour = f.pal.at (0.4f); ms.fillAlpha = 0; ms.edgeAlpha = 0.5f * k; ms.edgeWidth = 1.0f;
        rc.mesh (meshFor (Shape::Wireframe, 6), { 0, 0, -4 }, Vec3 { 1, 1, 1 } * (6.0f + 18.0f * t), { t * 2.0f, t * 3.0f, 0 }, ms);
    }
}
} // namespace eidolon
