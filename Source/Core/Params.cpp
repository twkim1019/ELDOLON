#include "Params.h"
#include <cmath>
#include <cstring>

namespace eidolon
{
namespace
{
const ParamInfo kTable[] = {
   #define EIDOLON_INFO(name, key, label, group, kind, mn, mx, def, choices, mut, flags, tip) \
        { P::name, key, label, PG::group, PK::kind, (float) (mn), (float) (mx), (float) (def), choices, MG::mut, (uint8_t) (flags), tip },
    EIDOLON_PARAMS (EIDOLON_INFO)
   #undef EIDOLON_INFO
};
static_assert (sizeof (kTable) / sizeof (kTable[0]) == (size_t) kNumParams, "parameter table mismatch");
} // namespace

const ParamInfo& paramInfo (P p)
{
    const int i = (int) p;
    return kTable[(i >= 0 && i < kNumParams) ? i : 0];
}

int ParamInfo::numChoices() const
{
    if (kind != PK::Choice || choices == nullptr || *choices == 0) return 0;
    int n = 1;
    for (const char* c = choices; *c; ++c) if (*c == '|') ++n;
    return n;
}

std::string ParamInfo::choiceName (int index) const
{
    if (kind != PK::Choice || choices == nullptr) return {};
    int i = 0;
    const char* start = choices;
    for (const char* c = choices;; ++c)
    {
        if (*c == '|' || *c == 0)
        {
            if (i == index) return std::string (start, (size_t) (c - start));
            if (*c == 0) break;
            ++i;
            start = c + 1;
        }
    }
    return {};
}

float ParamInfo::sanitise (float v) const
{
    if (! std::isfinite (v)) v = defV;
    v = v < minV ? minV : (v > maxV ? maxV : v);
    switch (kind)
    {
        case PK::Bool:   return v >= 0.5f ? 1.0f : 0.0f;
        case PK::Int:
        case PK::Choice: return (float) std::lround (v);
        case PK::Float:  break;
    }
    return v;
}

int findParamByKey (std::string_view key)
{
    for (int i = 0; i < kNumParams; ++i)
        if (key == kTable[i].key) return i;
    return -1;
}

const char* groupName (PG g)
{
    static const char* names[] = { "Layers", "Piano Universe", "Trails", "Constellation", "Flow", "Pulse", "Spectrum",
                                   "Orbit", "Scenes", "Camera", "Look & Light", "Particles", "Motion", "Rendering",
                                   "MIDI Input", "Analysis", "Dashboard", "Capture" };
    const int i = (int) g;
    return (i >= 0 && i < (int) PG::Count) ? names[i] : "";
}

const char* mutationGroupName (MG m)
{
    static const char* names[] = { "None", "Geometry", "Color", "Motion", "Camera", "Material", "Particle", "Trail", "Spatial" };
    const int i = (int) m;
    return (i >= 0 && i < (int) MG::Count) ? names[i] : "";
}

void ParamSet::resetToDefaults()
{
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = kTable[i].defV;
}
} // namespace eidolon
