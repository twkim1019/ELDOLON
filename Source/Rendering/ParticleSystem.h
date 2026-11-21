#pragma once
// EIDOLON — fixed-pool particle system (no allocation after construction).
#include "RenderContext.h"
#include <vector>

namespace eidolon
{
struct Particle
{
    Vec3 p, v;
    float life = 0, maxLife = 1, size = 0.1f, spin = 0;
    juce::uint32 argb = 0xffffffff;
};

class ParticleSystem
{
public:
    static constexpr int kPoolSize = 20000;

    ParticleSystem() { pool.resize (kPoolSize); }
    void setLimit (int n) { limit = clampv (n, 0, kPoolSize); if (live > limit) live = limit; }
    int count() const { return live; }
    int capacity() const { return limit; }

    void emit (const Vec3& p, const Vec3& v, float life, float size, juce::Colour c);
    void burst (const Vec3& p, int count, float speed, float life, float size, juce::Colour c, Rng& rng, float spread = 1.0f);
    void update (float dt, float gravity, float turbulence, float drag, float time);
    void render (RenderContext& rc, float alphaMul, float sizeMul) const;
    void clear() { live = 0; }

private:
    std::vector<Particle> pool;
    int live = 0, limit = 6000;
};
} // namespace eidolon
