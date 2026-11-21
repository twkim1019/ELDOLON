#pragma once
// EIDOLON — small math toolkit (vectors, easing, smoothing, value noise, reproducible RNG).
// Header-only and JUCE-free so the musical core and its tests do not depend on JUCE.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace eidolon
{
constexpr float kPi  = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

template <typename T> constexpr T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float saturate (float v) { return clampv (v, 0.0f, 1.0f); }
inline float smoothstepf (float e0, float e1, float x)
{
    const float t = saturate ((x - e0) / (e1 - e0 == 0.0f ? 1e-6f : (e1 - e0)));
    return t * t * (3.0f - 2.0f * t);
}
inline float easeOutCubic (float t) { t = saturate (t); const float u = 1.0f - t; return 1.0f - u * u * u; }
inline float easeInOut (float t) { t = saturate (t); return t < 0.5f ? 4 * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f; }

/** Frame-rate independent exponential smoothing coefficient for time constant `tau` (seconds). */
inline float smoothCoef (float dt, float tau) { return tau <= 1e-5f ? 1.0f : 1.0f - std::exp (-dt / tau); }

struct Vec3
{
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3 (float X, float Y, float Z) : x (X), y (Y), z (Z) {}
    Vec3 operator+ (const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator- () const { return { -x, -y, -z }; }
    Vec3& operator+= (const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-= (const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*= (float s) { x *= s; y *= s; z *= s; return *this; }
    float length() const { return std::sqrt (x * x + y * y + z * z); }
    Vec3 normalised() const { const float l = length(); return l > 1e-9f ? Vec3 { x / l, y / l, z / l } : Vec3 { 0, 0, 0 }; }
};
inline float dot (const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross (const Vec3& a, const Vec3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline Vec3 lerpv (const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline Vec3 rotateY (const Vec3& v, float a) { const float c = std::cos (a), s = std::sin (a); return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c }; }
inline Vec3 rotateX (const Vec3& v, float a) { const float c = std::cos (a), s = std::sin (a); return { v.x, v.y * c - v.z * s, v.y * s + v.z * c }; }
inline Vec3 rotateZ (const Vec3& v, float a) { const float c = std::cos (a), s = std::sin (a); return { v.x * c - v.y * s, v.x * s + v.y * c, v.z }; }

/** Reproducible PRNG (SplitMix64 seeding + xorshift64*). Identical sequences on every compiler / OS,
    which std:: distributions do not guarantee — Visual DNA mutations depend on this. */
class Rng
{
public:
    explicit Rng (uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed (seed); }
    void reseed (uint64_t seed)
    {
        uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        state = (z ^ (z >> 31)) | 1ull;
    }
    uint64_t next()
    {
        state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
        return state * 0x2545F4914F6CDD1Dull;
    }
    float nextFloat() { return (float) (next() >> 40) * (1.0f / 16777216.0f); }          // [0,1)
    float range (float lo, float hi) { return lo + (hi - lo) * nextFloat(); }
    float bipolar() { return nextFloat() * 2.0f - 1.0f; }
    int nextInt (int n) { return n <= 0 ? 0 : (int) ((next() >> 33) % (uint64_t) n); }
    uint64_t state = 1;
};

/** Cheap smooth 1-D / 3-D value noise in [-1, 1] (deterministic). */
inline float hash11 (float n) { const float s = std::sin (n * 127.1f) * 43758.5453f; return (s - std::floor (s)) * 2.0f - 1.0f; }
inline float noise1 (float x)
{
    const float i = std::floor (x), f = x - i;
    const float u = f * f * (3.0f - 2.0f * f);
    return lerpf (hash11 (i), hash11 (i + 1.0f), u);
}
inline float noise3 (float x, float y, float z)
{
    return (noise1 (x + 31.7f * y + 17.3f * z) + noise1 (y * 1.37f - z * 0.71f + 11.1f) + noise1 (z * 1.13f + x * 0.53f + 5.3f)) * (1.0f / 3.0f);
}

inline int popcount12 (uint16_t m) { int c = 0; for (int i = 0; i < 12; ++i) c += (m >> i) & 1; return c; }
inline uint16_t rotateMask (uint16_t m, int semis)   // interval mask relative to `semis`
{
    semis = ((semis % 12) + 12) % 12;
    return (uint16_t) (((m >> semis) | (m << (12 - semis))) & 0x0FFF);
}
} // namespace eidolon
