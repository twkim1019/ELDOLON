#include "ParticleSystem.h"

namespace eidolon
{
void ParticleSystem::emit (const Vec3& p, const Vec3& v, float life, float size, juce::Colour c)
{
    if (live >= limit || life <= 0.0f) return;
    auto& q = pool[(size_t) live++];
    q.p = p; q.v = v; q.life = life; q.maxLife = life; q.size = size; q.argb = c.getARGB(); q.spin = 0;
}

void ParticleSystem::burst (const Vec3& p, int n, float speed, float life, float size, juce::Colour c, Rng& rng, float spread)
{
    for (int i = 0; i < n && live < limit; ++i)
    {
        Vec3 d { rng.bipolar(), rng.bipolar() * spread, rng.bipolar() };
        d = d.normalised() * (speed * (0.3f + 0.7f * rng.nextFloat()));
        emit (p, d, life * (0.5f + 0.5f * rng.nextFloat()), size * (0.6f + 0.8f * rng.nextFloat()),
              c.withRotatedHue (rng.bipolar() * 0.03f));
    }
}

void ParticleSystem::update (float dt, float gravity, float turbulence, float drag, float time)
{
    const float damp = std::exp (-drag * dt);
    for (int i = 0; i < live;)
    {
        auto& q = pool[(size_t) i];
        q.life -= dt;
        if (q.life <= 0.0f) { q = pool[(size_t) --live]; continue; }
        if (turbulence > 0.0f)
        {
            const float tx = noise3 (q.p.x * 0.35f, q.p.y * 0.35f + time * 0.4f, q.p.z * 0.35f);
            const float ty = noise3 (q.p.y * 0.35f + 11.0f, q.p.z * 0.35f, q.p.x * 0.35f - time * 0.3f);
            const float tz = noise3 (q.p.z * 0.35f - 7.0f, q.p.x * 0.35f + time * 0.5f, q.p.y * 0.35f);
            q.v += Vec3 { tx, ty, tz } * (turbulence * 3.0f * dt);
        }
        q.v.y -= gravity * 2.5f * dt;
        q.v *= damp;
        q.p += q.v * dt;
        ++i;
    }
}

void ParticleSystem::render (RenderContext& rc, float alphaMul, float sizeMul) const
{
    for (int i = 0; i < live; ++i)
    {
        const auto& q = pool[(size_t) i];
        const auto pr = rc.project (q.p);
        if (! pr.visible) continue;
        const float t = q.life / q.maxLife;
        const float fade = t < 0.8f ? t / 0.8f : (1.0f - t) / 0.2f + 0.0f;
        const float a = juce::jlimit (0.0f, 1.0f, std::min (1.0f, fade * 1.2f)) * alphaMul;
        const float r = q.size * sizeMul * pr.scale;
        const juce::Colour c (q.argb);
        if (r < 1.4f) rc.point (pr.x, pr.y, std::max (0.6f, r), c, a);
        else
        {
            rc.glow (pr.x, pr.y, std::min (r * 2.6f, 80.0f), c, a * 0.55f);
            rc.point (pr.x, pr.y, std::min (r * 0.45f, 6.0f), c.brighter (0.5f), a * 0.9f);
        }
    }
}
} // namespace eidolon
