#include "ScaleDetector.h"
#include "ChordDetector.h"
#include "../Core/MathUtil.h"
#include <cmath>
#include <initializer_list>

namespace eidolon
{
namespace
{
// Krumhansl-Kessler key profiles.
constexpr float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
constexpr float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

uint16_t mk (std::initializer_list<int> iv) { uint16_t m = 0; for (int i : iv) m = (uint16_t) (m | (1u << i)); return m; }

float pearson (const std::array<float, 12>& h, const float* profile, int root)
{
    float mh = 0, mp = 0;
    for (int i = 0; i < 12; ++i) { mh += h[(size_t) i]; mp += profile[i]; }
    mh /= 12.0f; mp /= 12.0f;
    float num = 0, dh = 0, dp = 0;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = h[(size_t) pc] - mh;
        const float b = profile[(pc - root + 12) % 12] - mp;
        num += a * b; dh += a * a; dp += b * b;
    }
    return (dh > 1e-9f && dp > 1e-9f) ? num / std::sqrt (dh * dp) : 0.0f;
}
} // namespace

const std::vector<ScaleType>& ScaleDetector::scales()
{
    static const std::vector<ScaleType> s = {
        { "Major",            mk ({ 0, 2, 4, 5, 7, 9, 11 }), 0 },
        { "Natural Minor",    mk ({ 0, 2, 3, 5, 7, 8, 10 }), 1 },
        { "Harmonic Minor",   mk ({ 0, 2, 3, 5, 7, 8, 11 }), 1 },
        { "Melodic Minor",    mk ({ 0, 2, 3, 5, 7, 9, 11 }), 1 },
        { "Major Pentatonic", mk ({ 0, 2, 4, 7, 9 }),        0 },
        { "Minor Pentatonic", mk ({ 0, 3, 5, 7, 10 }),       1 },
        { "Blues",            mk ({ 0, 3, 5, 6, 7, 10 }),    1 },
        { "Chromatic",        0x0FFF,                         -1 },
        { "Dorian",           mk ({ 0, 2, 3, 5, 7, 9, 10 }), 1 },
        { "Custom",           0,                              -1 },
    };
    return s;
}

void ScaleDetector::addNote (int pitch, float weight)
{
    if (weight > 0.0f) hist[(size_t) (((pitch % 12) + 12) % 12)] += weight;
}

void ScaleDetector::decay (double dt)
{
    const float k = (float) std::exp (-dt / std::max (0.5, tau));
    for (auto& h : hist) h *= k;
}

float ScaleDetector::totalWeight() const
{
    float t = 0; for (auto h : hist) t += h; return t;
}

ScaleInfo ScaleDetector::estimate (bool preferFlats) const
{
    ScaleInfo info;
    const float total = totalWeight();
    if (total < 0.25f) return info;
    for (int i = 0; i < 12; ++i) info.histogram[(size_t) i] = hist[(size_t) i] / total;

    const auto& S = scales();
    std::array<ScaleCandidate, 3> top {};
    for (auto& c : top) c.score = -1e9f;

    for (int si = 0; si < (int) S.size(); ++si)
    {
        const uint16_t mask = si == kCustomIndex ? customMask : S[(size_t) si].mask;
        if (mask == 0) continue;
        const int size = popcount12 (mask);
        for (int root = 0; root < 12; ++root)
        {
            float in = 0;
            for (int i = 0; i < 12; ++i) if (mask & (1u << i)) in += hist[(size_t) ((root + i) % 12)];
            const float fit = in / total;
            const float kk = S[(size_t) si].profile == 0 ? pearson (hist, kMajorProfile, root)
                           : S[(size_t) si].profile == 1 ? pearson (hist, kMinorProfile, root) : 0.0f;
            const float tonic = (hist[(size_t) root] + 0.5f * hist[(size_t) ((root + 7) % 12)]) / total;
            const float score = fit - 0.035f * (float) (size - 5) + 0.2f * kk + 0.15f * tonic
                              + (si == kCustomIndex ? 0.03f : 0.0f);
            ScaleCandidate c { si, root, score, fit };
            for (size_t k = 0; k < top.size(); ++k)
                if (c.score > top[k].score) { for (size_t j = top.size() - 1; j > k; --j) top[j] = top[j - 1]; top[k] = c; break; }
        }
    }

    if (top[0].scaleIndex < 0) return info;
    info.valid = true;
    info.top = top;
    info.scaleIndex = top[0].scaleIndex;
    info.root = top[0].root;
    const float margin = top[1].scaleIndex >= 0 ? top[0].score - top[1].score : 0.5f;
    info.confidence = saturate (0.25f + margin * 4.0f) * saturate (total / 6.0f) * saturate (top[0].fit * 1.1f);
    info.name = std::string (ChordDetector::pitchName (info.root, preferFlats)) + " " + S[(size_t) info.scaleIndex].name;
    return info;
}
} // namespace eidolon
