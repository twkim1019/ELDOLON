#pragma once
// EIDOLON — cinematic scene system: ten procedural environments, musical transition triggers,
// eight transition types, and per-scene layer combinations for the Cinematic mode.
#include "../Visualization/VisualWorld.h"
#include <array>

namespace eidolon
{
constexpr int kNumScenes = 10;
const char* sceneName (int index);

struct SceneDrawParams
{
    float alpha = 1, scale = 1, rotation = 0, dissolve = 0;
};

struct SceneInputs
{
    float time = 0, energy = 0, density = 0, polyphony = 0, beatPhase = 0, barPhase = 0;
    bool beatTick = false, chordChanged = false;
    std::array<float, 12> pcActivity {};
    float registerNorm = 0, contour = 0, intensity = 1, sensitivity = 1, detail = 1;
    int lastInterval = 0, chordType = -1, chordRoot = -1;
    float chordPulse = 0;          // decays after a chord change
    uint32_t seed = 1;
    const Palette* pal = nullptr;
    struct Ripple { float x = 0, z = 0, born = -100, strength = 0; };
    std::array<Ripple, 16> ripples {};
};

/** Draws one scene. Implemented in SceneRenderers.cpp. */
void drawScene (int scene, RenderContext& rc, const SceneInputs& in, const SceneDrawParams& dp);

class SceneManager
{
public:
    void update (const FrameContext& f, float advanceValue);
    void render (RenderContext& rc, const FrameContext& f);

    void requestScene (int s);
    void next();
    int current() const { return cur; }
    int previous() const { return prev; }
    float progress() const { return t; }
    bool transitioning() const { return t < 1.0f; }
    bool changedThisFrame() const { return changedFlag; }

    /** Cinematic-mode layer weights: piano, constellation, flow, pulse, spectrum, orbit. */
    std::array<float, 6> layerWeights() const;

private:
    int chooseNext (const FrameContext& f);
    bool evaluateTrigger (const FrameContext& f);
    void startTransition (int target, int type, double now, float duration);

    int cur = 0, prev = 0, transType = 0, paramScene = -1;
    float t = 1.0f, duration = 3.5f;
    double lastChange = -1e9;
    bool changedFlag = false, levelArmed = true, particleBurstPending = false, pendingNext = false;
    int pendingTarget = -1;
    float lastAdvance = 0, energySmooth = 0, lastRegister = 0;
    int notesSince = 0, lastScaleRoot = -1;
    Rng rng { 0x5CE7E5u };
    SceneInputs in;
};
} // namespace eidolon
