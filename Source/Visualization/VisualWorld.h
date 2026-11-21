#pragma once
// EIDOLON — shared visual data model: pooled note entities, particles and the per-frame context
// every visualization layer reads. All layers operate on this one model.
#include "../Core/Params.h"
#include "../Intelligence/MusicalAnalyzer.h"
#include "../MIDI/NoteTracker.h"
#include "../Rendering/Palette.h"
#include "../Rendering/ParticleSystem.h"
#include <array>
#include <vector>

namespace eidolon
{
struct VisualNote
{
    struct Sample { double t = 0; float y = 0, z = 0; };

    bool alive = false;
    uint32_t id = 0;
    uint8_t channel = 0, pitch = 60, velocity = 100;
    int voice = -1;
    int chordGroup = 0;
    double onTime = 0, keyUpTime = -1, releaseTime = -1;
    float seed = 0;
    Vec3 offset, physVel, rot, spin;
    Vec3 pos;              // piano-universe head position (updated by that layer, used by camera / others)
    float drawScale = 1;
    float bend = 0;        // last pitch bend (-1..1) seen while sounding
    juce::uint32 colour = 0xffffffff;
    std::array<Sample, 64> trail {};
    int trailCount = 0, trailHead = 0;
    double lastSample = -1;
    bool burstDone = false;

    bool sounding() const { return releaseTime < 0; }
    double heldUntil (double now) const { return releaseTime >= 0 ? releaseTime : now; }
    /** 1 while sounding, then fades to 0 over `release` seconds. */
    float life (double now, float release) const
    {
        if (releaseTime < 0) return 1.0f;
        return clampv (1.0f - (float) ((now - releaseTime) / std::max (0.01f, release)), 0.0f, 1.0f);
    }
    void addSample (double t, float y, float z)
    {
        trail[(size_t) trailHead] = { t, y, z };
        trailHead = (trailHead + 1) % (int) trail.size();
        if (trailCount < (int) trail.size()) ++trailCount;
        lastSample = t;
    }
    const Sample& sample (int i) const   // 0 = oldest
    {
        return trail[(size_t) ((trailHead - trailCount + i + (int) trail.size() * 2) % (int) trail.size())];
    }
};

class VisualWorld
{
public:
    static constexpr int kMaxNotes = 1024;
    VisualWorld() { notes.resize (kMaxNotes); }

    VisualNote* spawn();
    VisualNote* find (uint32_t id);
    int liveCount() const { return live; }
    void recount();
    void clear() { for (auto& n : notes) n.alive = false; live = 0; particles.clear(); }

    std::vector<VisualNote> notes;
    ParticleSystem particles;
    int chordGroupCounter = 0;
    double lastSpawn = -1;

private:
    int live = 0;
};

struct FrameContext
{
    double now = 0;            // engine clock (seconds, same clock as note timestamps)
    float dt = 0.016f;
    float time = 0;            // animation time (scaled by global time scale)
    const ParamSet& p;
    const AnalysisSnapshot& a;
    const NoteTracker& tracker;
    const MusicalStatistics& stats;
    double statsWindow = 5.0;
    const std::vector<NoteChange>& changes;
    VisualWorld& world;
    const Palette& pal;
    Rng& rng;
    float energy = 1;          // global energy multiplier
    float activity = 0;        // 0..1 musical activity
    float particleScale = 1, trailScale = 1, detailScale = 1, motionQuality = 1;
    std::array<float, 7> layerOpacity {};   // piano, const, flow, pulse, spectrum, orbit, scene (effective)
    std::array<float, 7> layerScale { 1, 1, 1, 1, 1, 1, 1 };
    int chordRoot = -1;
};

class VisualLayer
{
public:
    virtual ~VisualLayer() = default;
    virtual void update (const FrameContext& f) = 0;
    virtual void render (RenderContext& rc, const FrameContext& f) = 0;
    virtual void reset() {}
};

// ---------------------------------------------------------------------------------- layers
class PianoUniverseLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
    static Vec3 notePosition (const VisualNote& n, double t, const FrameContext& f, float bend);
private:
    std::vector<VisualNote*> order;
    std::vector<Vec3> pts;
    std::vector<float> widths;
};

class ConstellationLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
    void reset() override { ghosts.clear(); }
private:
    struct Node { const VisualNote* n; Vec3 p; float a; float size; juce::Colour c; };
    struct Ghost { std::vector<Vec3> pts; juce::Colour c; double born; std::string name; int shape; };
    Vec3 nodePosition (const VisualNote& n, const FrameContext& f) const;
    void drawChord (RenderContext& rc, const FrameContext& f, const std::vector<Vec3>& pts, juce::Colour c, float a,
                    int shape, const std::string& label, int rootIndex);
    std::vector<Node> nodes;
    std::vector<Ghost> ghosts;
    std::vector<Vec3> scratch;
};

class FlowLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
    void reset() override { for (auto& v : voices) v.clear(); }
private:
    struct Point { double t; float pitch; float vel; int interval; };
    std::array<std::vector<Point>, 8> voices;
    std::vector<Vec3> pts;
    std::vector<float> widths;
};

class PulseLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
    void reset() override { count = 0; envelope = 0; }
private:
    struct Pulse { double born; float strength, hue, seed, ellipse, rot, deform; int sides; };
    void trigger (const FrameContext& f, float strength, float hue);
    std::array<Pulse, 96> pulses {};
    int count = 0;
    float envelope = 0, lastCC = 0, polyLevel = 0, osc = 0;
    double lastTrigger = -1;
};

class SpectrumLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
private:
    std::array<float, 128> bars {};
    std::array<float, 12 * 48> heat {};
    int lastView = -1;
};

class OrbitLayer : public VisualLayer
{
public:
    void update (const FrameContext& f) override;
    void render (RenderContext& rc, const FrameContext& f) override;
private:
    float spin = 0;
    std::array<float, 12> pcGlow {};
};
} // namespace eidolon
