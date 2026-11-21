#include "MutationEngine.h"
#include "../Core/MathUtil.h"
#include <cstdio>

namespace eidolon
{
namespace
{
float reflect01 (float v)
{
    for (int i = 0; i < 4 && (v < 0.0f || v > 1.0f); ++i) v = v < 0.0f ? -v : 2.0f - v;
    return saturate (v);
}

uint32_t mixSeed (uint32_t a, uint32_t b)
{
    uint64_t z = ((uint64_t) a << 32) ^ b ^ 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t) (z ^ (z >> 31));
}
} // namespace

VisualDNA MutationEngine::mutate (const VisualDNA& base, const MutationSettings& s)
{
    VisualDNA out = base;
    Rng rng (mixSeed (s.seed, 0xE1D0u));
    const float k = saturate (s.intensity);

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        // draw the random numbers unconditionally so preserved groups do not shift other groups' results
        const float r1 = rng.nextFloat(), r2 = rng.bipolar(), r3 = rng.nextFloat();
        if (! info.isDNA() || info.mutation == MG::None) continue;
        if (s.preserve[(size_t) info.mutation]) continue;
        const float cur = base.params.v[(size_t) i];
        switch (info.kind)
        {
            case PK::Float:
            case PK::Int:
            {
                const float n = reflect01 (info.normalise (cur) + r2 * k * 0.5f);
                out.params.set ((P) i, info.denormalise (n));
                break;
            }
            case PK::Choice:
                if (r1 < k * 0.35f) out.params.set ((P) i, (float) (int) (r3 * (float) info.numChoices()));
                break;
            case PK::Bool:
                if (r1 < k * 0.15f) out.params.set ((P) i, cur >= 0.5f ? 0.0f : 1.0f);
                break;
        }
    }

    if (s.mutateMappings && ! s.preserveMappings)
    {
        std::vector<int> modTargets;
        for (int i = 0; i < kNumParams; ++i) if (paramInfo (i).isModulatable() && paramInfo (i).kind == PK::Float) modTargets.push_back (i);
        for (auto& m : out.mappings)
        {
            const float a = rng.bipolar(), b = rng.bipolar(), c = rng.nextFloat(), d = rng.nextFloat(), e = rng.bipolar();
            m.outMin = reflect01 (m.outMin + a * k * 0.3f);
            m.outMax = reflect01 (m.outMax + b * k * 0.3f);
            m.sensitivity = clampv (m.sensitivity * (1.0f + e * k * 0.5f), 0.1f, 4.0f);
            if (c < k * 0.25f) m.curve = (MapCurve) (int) (d * 4.0f);
            if (! modTargets.empty() && d < k * 0.2f) m.target = modTargets[(size_t) ((int) (c * 997.0f) % (int) modTargets.size())];
            m.resetRuntime();
        }
    }

    out.seed = s.seed;
    char name[160];
    std::snprintf (name, sizeof (name), "%s ~%u", base.name.substr (0, 100).c_str(), (unsigned) (s.seed % 100000u));
    out.name = name;
    return out;
}

std::vector<VisualDNA> MutationEngine::variations (const VisualDNA& base, const MutationSettings& s, int count)
{
    std::vector<VisualDNA> v;
    for (int i = 0; i < count; ++i)
    {
        MutationSettings si = s;
        si.seed = mixSeed (s.seed, (uint32_t) (i + 1) * 7919u);
        v.push_back (mutate (base, si));
    }
    return v;
}

VisualDNA MutationEngine::randomize (uint32_t seed, const VisualDNA& base, const std::array<bool, (size_t) MG::Count>& preserve)
{
    MutationSettings s;
    s.intensity = 1.0f;
    s.seed = seed;
    s.preserve = preserve;
    VisualDNA d = mutate (base, s);
    Rng rng (mixSeed (seed, 0xBADC0DEu));
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        const float r = rng.nextFloat();
        if (! info.isDNA() || info.mutation == MG::None || preserve[(size_t) info.mutation]) continue;
        if (info.kind == PK::Choice) d.params.set ((P) i, (float) (int) (r * (float) info.numChoices()));
    }
    char name[64];
    std::snprintf (name, sizeof (name), "Random %u", (unsigned) (seed % 100000u));
    d.name = name;
    return d;
}

void DNAHistory::push (const VisualDNA& d)
{
    if (index >= 0 && index + 1 < (int) stack.size()) stack.resize ((size_t) index + 1);
    stack.push_back (d);
    if (stack.size() > kMax) stack.erase (stack.begin());
    index = (int) stack.size() - 1;
}
} // namespace eidolon
