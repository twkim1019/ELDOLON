#pragma once
// EIDOLON — probabilistic scale / key estimation from a decaying pitch-class histogram.
// Output is an estimate with a confidence, never a definitive interpretation.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace eidolon
{
struct ScaleType
{
    const char* name;
    uint16_t mask;     // pitch classes relative to the root
    int profile;       // 0 = major key profile, 1 = minor key profile, -1 = none
};

struct ScaleCandidate
{
    int scaleIndex = -1;
    int root = -1;
    float score = 0.0f;
    float fit = 0.0f;  // share of the observed weight inside the scale (compatibility)
};

struct ScaleInfo
{
    bool valid = false;
    int scaleIndex = -1;
    int root = -1;
    float confidence = 0.0f;
    std::string name;                        // "D Harmonic Minor"
    std::array<ScaleCandidate, 3> top {};    // best three interpretations
    std::array<float, 12> histogram {};      // normalized pitch-class distribution
};

class ScaleDetector
{
public:
    static constexpr int kCustomIndex = 9;
    static const std::vector<ScaleType>& scales();   // index 9 is "Custom" (mask set at runtime)

    void setTimeConstant (double seconds) { tau = seconds; }
    void setCustomMask (uint16_t m) { customMask = m; }

    void addNote (int pitch, float weight);           // weight ~ velocity * duration
    void decay (double dt);
    ScaleInfo estimate (bool preferFlats) const;
    float totalWeight() const;
    void reset() { hist.fill (0.0f); }

private:
    std::array<float, 12> hist {};
    double tau = 20.0;
    uint16_t customMask = 0;
};
} // namespace eidolon
