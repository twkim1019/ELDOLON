#include "VisualizationEngine.h"
#include "../DNA/VisualDNA.h"
#include "../UI/Theme.h"

namespace eidolon
{
// ------------------------------------------------------------------------------------------------- world
VisualNote* VisualWorld::spawn()
{
    VisualNote* oldestReleased = nullptr;
    VisualNote* oldest = nullptr;
    for (auto& n : notes)
    {
        if (! n.alive) { n = VisualNote {}; n.alive = true; ++live; return &n; }
        if (n.releaseTime >= 0 && (oldestReleased == nullptr || n.releaseTime < oldestReleased->releaseTime)) oldestReleased = &n;
        if (oldest == nullptr || n.onTime < oldest->onTime) oldest = &n;
    }
    VisualNote* victim = oldestReleased != nullptr ? oldestReleased : oldest;   // overflow: recycle
    *victim = VisualNote {};
    victim->alive = true;
    return victim;
}

VisualNote* VisualWorld::find (uint32_t id)
{
    for (auto& n : notes) if (n.alive && n.id == id) return &n;
    return nullptr;
}

void VisualWorld::recount()
{
    live = 0;
    for (auto& n : notes) if (n.alive) ++live;
}

// ------------------------------------------------------------------------------------------------- engine
VisualizationEngine::VisualizationEngine()
{
    glowImage = RenderContext::createGlowImage (64);
    glowSprites = RenderContext::createGlowSprites();
    for (int s = 0; s < (int) Shape::Count; ++s) for (int d = 3; d <= 12; ++d) meshFor ((Shape) s, d);   // warm the cache
    noChanges.reserve (1);
}

void VisualizationEngine::clear()
{
    world.clear();
    piano.reset(); constellation.reset(); flow.reset(); pulse.reset(); spectrum.reset(); orbit.reset();
}

void VisualizationEngine::computeQuality (const ParamSet& p)
{
    static const float presetScale[] = { 0.35f, 0.6f, 0.85f, 1.0f };
    const float q = presetScale[clampv (p.geti (P::RenderQuality), 0, 3)];
    const float a = p.getb (P::RenderAdaptive) ? adaptive : 1.0f;
    particleScale = q * a;
    trailScale = clampv (q * p.get (P::RenderTrailQ) * a + 0.15f, 0.15f, 1.0f);
    detailScale = clampv (q * p.get (P::RenderGeomQ) * a + 0.1f, 0.1f, 1.0f);
    motionQuality = clampv (q * p.get (P::RenderMotionQ), 0.2f, 1.0f);
    world.particles.setLimit ((int) ((float) p.geti (P::RenderParticles) * std::max (0.2f, a)));
    st.quality = q * a;
}

void VisualizationEngine::reportFrameTime (float ms)
{
    st.renderMs = st.renderMs * 0.9f + ms * 0.1f;
    const float budget = 1000.0f / std::max (15.0f, params.get (P::RenderFps));
    const float total = st.renderMs + st.updateMs;
    if (total > budget * 1.5f) adaptive = std::max (0.15f, adaptive - 0.05f);
    else if (total > budget * 0.95f) adaptive = std::max (0.15f, adaptive - 0.02f);
    else if (total < budget * 0.6f) adaptive = std::min (1.0f, adaptive + 0.005f);
}

void VisualizationEngine::spawnFromChanges (const std::vector<NoteChange>& changes, double t)
{
    const int colorBy = params.geti (P::LookColorBy);
    for (const auto& c : changes)
    {
        switch (c.kind)
        {
            case NoteChange::Start:
            {
                VisualNote* n = world.spawn();
                n->id = c.id; n->channel = c.channel; n->pitch = c.pitch; n->velocity = c.velocity; n->voice = c.voice;
                n->onTime = std::min (c.time, t);
                n->seed = rng.nextFloat();
                n->rot = { rng.range (0, kTau), rng.range (0, kTau), rng.range (0, kTau) };
                n->spin = { rng.bipolar(), rng.bipolar() * 0.6f + 0.8f, rng.bipolar() * 0.4f };
                if (n->onTime - world.lastSpawn > 0.06) ++world.chordGroupCounter;
                n->chordGroup = world.chordGroupCounter;
                world.lastSpawn = n->onTime;
                n->colour = noteColour (palette, colorBy, c.pitch, c.channel, c.velocity, c.voice,
                                        lastA != nullptr && lastA->chord.valid ? lastA->chord.root : -1).getARGB();
                break;
            }
            case NoteChange::KeyUp:
                if (auto* n = world.find (c.id)) n->keyUpTime = std::min (c.time, t);
                break;
            case NoteChange::Release:
                if (auto* n = world.find (c.id))
                {
                    n->releaseTime = std::min (c.time, t);
                    if (n->keyUpTime < 0) n->keyUpTime = n->releaseTime;
                }
                break;
        }
    }
}

void VisualizationEngine::update (double nowIn, float dt, const ParamSet& p, const AnalysisSnapshot& a, const NoteTracker& tracker,
                                  const MusicalStatistics& stats, double statsWindow, const std::vector<NoteChange>& changes)
{
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    params = p;
    palette = makePalette (p);
    lastA = &a; lastT = &tracker; lastS = &stats; lastStatsWindow = statsWindow;
    now = nowIn;
    dt = clampv (dt, 0.0f, 0.1f);
    const float motion = p.get (P::MotionSpeed);
    animTime += dt * motion;
    computeQuality (p);

    const float target = saturate (a.noteDensity / 10.0f * 0.6f + (float) a.polyphony / 10.0f * 0.4f + a.recentVelocity / 127.0f * 0.2f);
    activity += (target - activity) * smoothCoef (dt, 0.6f);

    spawnFromChanges (changes, now);

    // effective layer opacities (cinematic mode: the scene decides the mix)
    const P onP[] = { P::PianoOn, P::ConstOn, P::FlowOn, P::PulseOn, P::SpecOn, P::OrbitOn, P::SceneOn };
    const P opP[] = { P::PianoOpacity, P::ConstOpacity, P::FlowOpacity, P::PulseOpacity, P::SpecOpacity, P::OrbitOpacity, P::SceneOpacity };
    const P scP[] = { P::PianoScale, P::ConstScale, P::FlowScale, P::PulseScale, P::SpecScale, P::OrbitScale, P::SceneScale };
    const bool cinematic = p.geti (P::Mode) == 6;
    const auto w = sceneMgr.layerWeights();
    for (int i = 0; i < 7; ++i)
    {
        float o = p.get (opP[i]);
        if (cinematic && i < 6) o *= w[(size_t) i];
        else if (! p.getb (onP[i])) o = 0.0f;
        if (cinematic && i == 6) o = p.get (opP[i]);
        opacity[(size_t) i] += (o - opacity[(size_t) i]) * smoothCoef (dt, 0.25f);
        scale[(size_t) i] = p.get (scP[i]);
    }

    FrameContext f { now, dt, animTime, p, a, tracker, stats, statsWindow, changes, world, palette, rng };
    f.energy = p.get (P::MotionEnergy);
    f.activity = activity;
    f.particleScale = particleScale; f.trailScale = trailScale; f.detailScale = detailScale; f.motionQuality = motionQuality;
    f.layerOpacity = opacity; f.layerScale = scale;
    f.chordRoot = a.chord.valid ? a.chord.root : -1;

    piano.update (f);                                   // always: positions feed camera + other layers
    if (opacity[1] > 0.001f) constellation.update (f);
    flow.update (f);                                    // keeps its own melodic history even when hidden
    if (opacity[3] > 0.001f) pulse.update (f);
    if (opacity[4] > 0.001f) spectrum.update (f);
    if (opacity[5] > 0.001f) orbit.update (f);
    sceneMgr.update (f, p.get (P::SceneAdvance));
    if (sceneMgr.changedThisFrame()) flash = 1.0f;
    flash *= std::exp (-dt * 2.0f);

    // particles
    if (p.getb (P::PartOn))
        world.particles.update (dt * motion, p.get (P::PartGravity), p.get (P::PartTurbulence), 0.6f, animTime);
    else
        world.particles.clear();

    // retire fully faded notes
    const float maxLife = std::max ({ p.get (P::PianoRelease), opacity[1] > 0.001f ? p.get (P::ConDuration) : 0.0f, 1.0f }) + 0.4f;
    for (auto& n : world.notes)
        if (n.alive && n.releaseTime >= 0 && now - n.releaseTime > maxLife) n.alive = false;
    world.recount();

    // camera
    CameraFocus focus;
    Vec3 sum; int cnt = 0; float maxD = 0; double newest = -1;
    for (const auto& n : world.notes)
    {
        if (! n.alive) continue;
        if (n.onTime > newest) { newest = n.onTime; focus.latest = n.pos; }
        if (! n.sounding()) continue;
        sum += n.pos; ++cnt;
    }
    if (cnt > 0)
    {
        focus.hasActive = true;
        focus.activeCentroid = sum * (1.0f / (float) cnt);
        for (const auto& n : world.notes) if (n.alive && n.sounding()) maxD = std::max (maxD, (n.pos - focus.activeCentroid).length());
        focus.activeSpread = maxD;
    }
    focus.musicalCenterY = (a.registerCenter - 60.0f) * 0.16f * p.get (P::PianoPitchSpread) * scale[0];
    focus.energy = activity;
    focus.chordChanged = a.chordChanged;
    focus.sceneChanged = sceneMgr.changedThisFrame();
    for (const auto& c : changes) if (c.kind == NoteChange::Start) focus.impulse = std::max (focus.impulse, (float) c.velocity / 127.0f);
    camCtl.update (dt, animTime, p, focus, cam);

    st.notes = world.liveCount();
    st.particles = world.particles.count();
    st.updateMs = st.updateMs * 0.9f + (float) (juce::Time::getMillisecondCounterHiRes() - t0) * 0.1f;
}

void VisualizationEngine::drawBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! gpuFrame)
    {
        // CPU path: the full-screen gradients only change slowly, so cache them
        const int bucketA = (int) (activity * 20.0f), bucketT = (int) ((std::sin (animTime * 0.05f) + 1.0f) * 25.0f);
        const juce::String key = juce::String ((int) area.getWidth()) + "x" + juce::String ((int) area.getHeight()) + ":" + juce::String::toHexString ((int) palette.bgTop.getARGB())
                               + juce::String::toHexString ((int) palette.bgBottom.getARGB()) + ":" + juce::String (bucketA) + ":" + juce::String (bucketT)
                               + ":" + juce::String::toHexString ((int) palette.at (0.15f).getARGB());
        if (key != bgKey || ! bgCache.isValid())
        {
            bgCache = juce::Image (juce::Image::RGB, std::max (1, (int) area.getWidth()), std::max (1, (int) area.getHeight()), false);
            juce::Graphics bg (bgCache);
            const bool was = gpuFrame;
            gpuFrame = true;   // paint the real gradients into the cache
            drawBackground (bg, bgCache.getBounds().toFloat());
            gpuFrame = was;
            bgKey = key;
        }
        g.drawImageAt (bgCache, (int) area.getX(), (int) area.getY());
        return;
    }
    juce::ColourGradient grad (palette.bgTop, area.getCentreX(), area.getY(), palette.bgBottom, area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (area);
    // a faint luminous horizon that breathes with activity
    const auto accent = palette.at (0.15f + 0.1f * std::sin (animTime * 0.05f));
    juce::ColourGradient halo (accent.withAlpha (0.07f + 0.08f * activity), area.getCentreX(), area.getCentreY() + area.getHeight() * 0.1f,
                               accent.withAlpha (0.0f), area.getCentreX() + area.getWidth() * 0.6f, area.getCentreY(), true);
    g.setGradientFill (halo);
    g.fillRect (area);
}

void VisualizationEngine::drawPost (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float vig = params.get (P::LookVignette);
    if (vig > 0.01f)
    {
        auto paintVignette = [&] (juce::Graphics& vg, juce::Rectangle<float> r)
        {
            juce::ColourGradient v (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                    juce::Colours::black.withAlpha (0.75f * vig), r.getX(), r.getY(), true);
            v.addColour (0.55, juce::Colours::transparentBlack);
            vg.setGradientFill (v);
            vg.fillRect (r);
        };
        if (gpuFrame) paintVignette (g, area);
        else
        {
            const juce::String key = juce::String ((int) area.getWidth()) + "x" + juce::String ((int) area.getHeight()) + ":" + juce::String (vig, 2);
            if (key != vignetteKey || ! vignetteCache.isValid())
            {
                vignetteCache = juce::Image (juce::Image::ARGB, std::max (1, (int) area.getWidth()), std::max (1, (int) area.getHeight()), true);
                juce::Graphics vg (vignetteCache);
                paintVignette (vg, vignetteCache.getBounds().toFloat());
                vignetteKey = key;
            }
            g.drawImageAt (vignetteCache, (int) area.getX(), (int) area.getY());
        }
    }
    const float tex = params.get (P::LookTexture);
    if (tex > 0.01f)
    {
        Rng grain ((uint64_t) (animTime * 30.0f) + 17);
        const int n = (int) (area.getWidth() * area.getHeight() / 2500.0f * tex);
        for (int i = 0; i < n; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.035f * tex + 0.03f * grain.nextFloat() * tex));
            g.fillRect (area.getX() + grain.nextFloat() * area.getWidth(), area.getY() + grain.nextFloat() * area.getHeight(), 1.0f, 1.0f);
        }
    }
    if (flash > 0.02f && params.geti (P::SceneTransType) == 7)
    {
        g.setColour (palette.at (0.3f).withAlpha (0.12f * flash));
        g.fillRect (area);
    }
}

void VisualizationEngine::render (juce::Graphics& g, juce::Rectangle<float> area, bool gpu)
{
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    gpuFrame = gpu;
    cam.setViewport (area);
    drawBackground (g, area);
    if (lastA == nullptr || lastT == nullptr || lastS == nullptr) { ++frames; return; }

    FrameContext f { now, 0.0f, animTime, params, *lastA, *lastT, *lastS, lastStatsWindow, noChanges, world, palette, rng };
    f.energy = params.get (P::MotionEnergy);
    f.activity = activity;
    f.particleScale = particleScale; f.trailScale = trailScale; f.detailScale = detailScale; f.motionQuality = motionQuality;
    f.layerOpacity = opacity; f.layerScale = scale;
    f.chordRoot = lastA->chord.valid ? lastA->chord.root : -1;

    RenderContext rc (g, cam, glowImage, &glowSprites);
    rc.bloom = params.get (P::LookBloom);
    rc.detail = detailScale;
    rc.brightness = params.get (P::LookBrightness);
    rc.texture = params.get (P::LookTexture);
    const float la = params.get (P::LookLightAngle) * kPi / 180.0f;
    rc.lightDir = Vec3 { std::cos (la) * 0.7f, 0.65f, std::sin (la) * 0.7f }.normalised();
    g.setImageResamplingQuality (params.getb (P::RenderAA) ? juce::Graphics::mediumResamplingQuality : juce::Graphics::lowResamplingQuality);

    auto layer = [&] (int i, VisualLayer& L)
    {
        if (opacity[(size_t) i] <= 0.003f) return;
        rc.alpha = opacity[(size_t) i];
        L.render (rc, f);
    };

    if (opacity[6] > 0.003f)
    {
        if (gpu) { rc.alpha = opacity[6]; sceneMgr.render (rc, f); }
        else
        {
            // CPU path: the soft procedural environment is rendered at half resolution and upscaled
            const int w = std::max (1, (int) area.getWidth() / 2), h = std::max (1, (int) area.getHeight() / 2);
            if (! sceneImage.isValid() || sceneImage.getWidth() != w || sceneImage.getHeight() != h)
                sceneImage = juce::Image (juce::Image::ARGB, w, h, true);
            else
                sceneImage.clear (sceneImage.getBounds());
            {
                juce::Graphics sg (sceneImage);
                sg.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (0.5f));
                RenderContext src (sg, cam, glowImage, &glowSprites);
                src.bloom = rc.bloom; src.detail = rc.detail * 0.8f; src.brightness = rc.brightness; src.lightDir = rc.lightDir;
                src.alpha = opacity[6];
                sceneMgr.render (src, f);
            }
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            g.drawImage (sceneImage, area);
            g.setImageResamplingQuality (params.getb (P::RenderAA) ? juce::Graphics::mediumResamplingQuality : juce::Graphics::lowResamplingQuality);
        }
    }
    if (params.geti (P::SpecPlacement) == 0) layer (4, spectrum);
    layer (5, orbit);
    layer (1, constellation);
    layer (2, flow);
    layer (0, piano);
    layer (3, pulse);
    if (params.getb (P::PartOn))
    {
        rc.alpha = 1.0f;
        world.particles.render (rc, 0.85f * params.get (P::LookBrightness), params.get (P::PartSize));
    }
    if (params.geti (P::SpecPlacement) != 0) layer (4, spectrum);
    drawPost (g, area);

    ++frames;
    const double t1 = juce::Time::getMillisecondCounterHiRes();
    if (lastFrameStamp > 0) st.fps = st.fps * 0.9f + (float) (1000.0 / std::max (1.0, t1 - lastFrameStamp)) * 0.1f;
    lastFrameStamp = t1;
    st.frameMs = st.frameMs * 0.9f + (float) (t1 - t0) * 0.1f;
}
} // namespace eidolon
