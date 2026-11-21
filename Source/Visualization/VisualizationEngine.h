#pragma once
// EIDOLON — visualization engine: owns the shared visual world, all layers, the scene system and the camera.
// update() runs on the engine (message) thread; render() paints with juce::Graphics (GPU via OpenGL when
// attached, CPU otherwise).
#include "VisualWorld.h"
#include "../Scenes/SceneManager.h"
#include <memory>

namespace eidolon
{
class VisualizationEngine
{
public:
    VisualizationEngine();

    void update (double now, float dt, const ParamSet& p, const AnalysisSnapshot& a, const NoteTracker& tracker,
                 const MusicalStatistics& stats, double statsWindow, const std::vector<NoteChange>& changes);
    void render (juce::Graphics& g, juce::Rectangle<float> area);
    void clear();

    Camera& camera() { return cam; }
    CameraController& cameraController() { return camCtl; }
    SceneManager& scenes() { return sceneMgr; }

    struct Stats
    {
        float frameMs = 0, updateMs = 0, renderMs = 0, fps = 0, quality = 1;
        int notes = 0, particles = 0;
    };
    const Stats& stats() const { return st; }
    void reportFrameTime (float ms);      // canvas measures the full paint time
    bool hasRendered() const { return frames > 0; }

private:
    void spawnFromChanges (const std::vector<NoteChange>& changes, double now);
    void computeQuality (const ParamSet& p);
    void drawBackground (juce::Graphics& g, juce::Rectangle<float> area);
    void drawPost (juce::Graphics& g, juce::Rectangle<float> area);

    VisualWorld world;
    PianoUniverseLayer piano;
    ConstellationLayer constellation;
    FlowLayer flow;
    PulseLayer pulse;
    SpectrumLayer spectrum;
    OrbitLayer orbit;
    SceneManager sceneMgr;
    Camera cam;
    CameraController camCtl;
    Rng rng { 0xE1D010u };
    juce::Image glowImage;

    // last frame inputs (render reads them; render and update are serialized)
    ParamSet params;
    Palette palette;
    const AnalysisSnapshot* lastA = nullptr;
    const NoteTracker* lastT = nullptr;
    const MusicalStatistics* lastS = nullptr;
    double lastStatsWindow = 5.0;
    std::vector<NoteChange> noChanges;
    std::array<float, 7> opacity {};
    std::array<float, 7> scale {};
    double now = 0;
    float animTime = 0, activity = 0, adaptive = 1.0f, flash = 0;
    float particleScale = 1, trailScale = 1, detailScale = 1, motionQuality = 1;
    Stats st;
    int frames = 0;
    double lastFrameStamp = 0;
};
} // namespace eidolon
