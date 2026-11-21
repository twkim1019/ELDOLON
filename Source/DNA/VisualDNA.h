#pragma once
// EIDOLON — Visual DNA: the complete visual identity of a scene (all DNA-flagged parameters,
// MIDI-to-visual mappings and the random seed). Serialized as JSON by DNASerializer.
#include "../Core/Params.h"
#include "../Mapping/MappingEngine.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eidolon
{
constexpr int kDNAFormatVersion = 1;

inline const char* const kPresetCategories[] = { "Favorites", "Recent", "Minimal", "Organic", "Geometric", "Cinematic",
                                                 "Celestial", "Abstract", "Experimental", "Custom" };
constexpr int kNumPresetCategories = 10;

struct VisualDNA
{
    std::string name = "Init";
    std::string category = "Custom";
    std::string author = "User";
    std::string description;
    uint32_t seed = 1;
    ParamSet params;                  // DNA-flagged entries are the preset; others are ignored on load
    std::vector<Mapping> mappings;

    /** Copies only the DNA-flagged parameters (keeps analysis / capture / MIDI settings of `target`). */
    void applyTo (ParamSet& target) const
    {
        for (int i = 0; i < kNumParams; ++i)
            if (paramInfo (i).isDNA()) target.v[(size_t) i] = params.v[(size_t) i];
    }
    void captureFrom (const ParamSet& source)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (paramInfo (i).isDNA()) params.v[(size_t) i] = source.v[(size_t) i];
    }
};

/** Layer mix implied by each primary mode (Custom Mix leaves the toggles alone). */
inline void applyModeLayers (ParamSet& p, int mode)
{
    if (mode < 0 || mode > 6) return;
    const P on[] = { P::PianoOn, P::ConstOn, P::FlowOn, P::PulseOn, P::SpecOn, P::OrbitOn };
    for (int i = 0; i < 6; ++i) p.set (on[i], i == mode ? 1.0f : 0.0f);
    p.set (P::SceneOn, 1.0f);
    if (mode == 4) p.set (P::SpecPlacement, 2.0f);
    p.set (P::Mode, (float) mode);
}
} // namespace eidolon
