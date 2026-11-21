#include "MappingEngine.h"
#include "../Core/MathUtil.h"
#include <cmath>

namespace eidolon
{
const char* mapSourceName (MapSource s)
{
    static const char* n[] = { "Note On", "Note Off", "Velocity", "Pitch", "Duration", "Channel", "CC", "Pitch Bend",
                               "Channel Pressure", "Poly Aftertouch", "Sustain", "Beat (clock)", "Bar (clock)",
                               "Polyphony", "Note Density", "Chord Change" };
    const int i = (int) s;
    return (i >= 0 && i < (int) MapSource::Count) ? n[i] : "?";
}
const char* mapCurveName (MapCurve c)
{
    static const char* n[] = { "Linear", "Exponential", "Logarithmic", "S-Curve", "Custom" };
    const int i = (int) c;
    return (i >= 0 && i < (int) MapCurve::Count) ? n[i] : "?";
}
const char* mapResponseName (MapResponse r)
{
    static const char* n[] = { "Direct", "Envelope", "Accumulate", "Toggle" };
    const int i = (int) r;
    return (i >= 0 && i < (int) MapResponse::Count) ? n[i] : "?";
}
const char* mapBlendName (MapBlend b)
{
    static const char* n[] = { "Replace", "Add", "Max", "Min" };
    const int i = (int) b;
    return (i >= 0 && i < (int) MapBlend::Count) ? n[i] : "?";
}

bool MappingEngine::isTriggerSource (MapSource s)
{
    return s == MapSource::NoteOn || s == MapSource::NoteOff || s == MapSource::ChordChange;
}

float applyCurve (const Mapping& m, float x)
{
    x = saturate (x);
    const float p = 1.0f + 3.0f * saturate (m.curveAmount);
    switch (m.curve)
    {
        case MapCurve::Linear:      return x;
        case MapCurve::Exponential: return std::pow (x, p);
        case MapCurve::Logarithmic: return 1.0f - std::pow (1.0f - x, p);
        case MapCurve::SCurve:      return x < 0.5f ? 0.5f * std::pow (2.0f * x, p) : 1.0f - 0.5f * std::pow (2.0f - 2.0f * x, p);
        case MapCurve::Custom:
        {
            const float pts[5] = { 0.0f, saturate (m.customPoints[0]), saturate (m.customPoints[1]), saturate (m.customPoints[2]), 1.0f };
            const float f = x * 4.0f;
            const int i = std::min (3, (int) f);
            return lerpf (pts[i], pts[i + 1], f - (float) i);
        }
        case MapCurve::Count: break;
    }
    return x;
}

float MappingEngine::rawSourceValue (const Mapping& m, const MappingInputs& in, bool& trigger)
{
    trigger = false;
    const bool any = m.channel <= 0 || m.channel > 16;
    const int ch = any ? 0 : m.channel - 1;
    auto perChannelMax = [&] (const std::array<float, 16>& a) { float v = 0; for (float x : a) v = std::max (v, x); return v; };

    switch (m.source)
    {
        case MapSource::NoteOn:
        {
            int count = 0; float vel = 0;
            if (any) { for (int c = 0; c < 16; ++c) { count += in.noteOns[(size_t) c]; vel = std::max (vel, in.noteOns[(size_t) c] ? in.lastVelocity[(size_t) c] : 0.0f); } }
            else { count = in.noteOns[(size_t) ch]; vel = in.lastVelocity[(size_t) ch]; }
            trigger = count > 0;
            return trigger ? std::max (0.05f, vel) : 0.0f;
        }
        case MapSource::NoteOff:
        {
            int count = 0;
            if (any) for (int c : in.noteOffs) count += c; else count = in.noteOffs[(size_t) ch];
            trigger = count > 0;
            return trigger ? 1.0f : 0.0f;
        }
        case MapSource::Velocity:   return any ? in.lastVelocityAny : in.lastVelocity[(size_t) ch];
        case MapSource::Pitch:      return any ? in.lastPitchAny : in.lastPitch[(size_t) ch];
        case MapSource::Duration:   return saturate (in.averageDuration / 4.0f);
        case MapSource::Channel:    return in.lastChannel;
        case MapSource::CC:
        {
            const int n = clampv (m.ccNumber, 0, 127);
            if (! any) return in.cc[(size_t) ch][(size_t) n];
            float v = 0; for (int c = 0; c < 16; ++c) v = std::max (v, in.cc[(size_t) c][(size_t) n]);
            return v;
        }
        case MapSource::PitchBend:
        {
            if (! any) return (in.bend[(size_t) ch] + 1.0f) * 0.5f;
            float b = 0; for (float x : in.bend) if (std::abs (x) > std::abs (b)) b = x;
            return (b + 1.0f) * 0.5f;
        }
        case MapSource::ChannelPressure: return any ? perChannelMax (in.pressure) : in.pressure[(size_t) ch];
        case MapSource::PolyAftertouch:  return any ? perChannelMax (in.polyAT) : in.polyAT[(size_t) ch];
        case MapSource::Sustain:
        {
            if (! any) return in.sustain[(size_t) ch] ? 1.0f : 0.0f;
            for (bool s : in.sustain) if (s) return 1.0f;
            return 0.0f;
        }
        case MapSource::Beat:        trigger = in.beatTick; return 1.0f - in.beatPhase;
        case MapSource::Bar:         trigger = in.barTick;  return 1.0f - in.barPhase;
        case MapSource::Polyphony:   return saturate ((float) in.polyphony / 10.0f);
        case MapSource::NoteDensity: return saturate (in.density / 16.0f);
        case MapSource::ChordChange: trigger = in.chordChanged; return trigger ? 1.0f : 0.0f;
        case MapSource::Count: break;
    }
    return 0.0f;
}

void MappingEngine::process (std::vector<Mapping>& maps, const MappingInputs& in, float dt, const ParamSet& base, ParamSet& out)
{
    out = base;
    if (maps.empty()) return;

    std::array<float, kNumParams> value {};
    std::array<bool, kNumParams> touched {};
    std::array<float, kNumParams> add {};
    for (int i = 0; i < kNumParams; ++i) value[(size_t) i] = base.norm ((P) i);

    // pass 1: evaluate every mapping, apply Replace in list order (last wins)
    for (auto& m : maps)
    {
        if (m.target < 0 || m.target >= kNumParams || ! paramInfo (m.target).isModulatable()) continue;
        bool trigger = false;
        float x = rawSourceValue (m, in, trigger);
        const float span = m.inMax - m.inMin;
        x = std::abs (span) > 1e-6f ? saturate ((x - m.inMin) / span) : (x >= m.inMin ? 1.0f : 0.0f);
        if (m.deadZone > 0.0f) x = x < m.deadZone ? 0.0f : (x - m.deadZone) / std::max (1e-4f, 1.0f - m.deadZone);
        x = saturate (x * m.sensitivity);
        if (m.invert) x = 1.0f - x;
        x = applyCurve (m, x);

        float y = x;
        switch (m.response)
        {
            case MapResponse::Direct: y = x; break;
            case MapResponse::Envelope:
            {
                const float k = std::exp (-dt / std::max (0.02f, m.decay));
                if (isTriggerSource (m.source) || m.source == MapSource::Beat || m.source == MapSource::Bar)
                    m.state = trigger ? std::max (m.state * k, x) : m.state * k;
                else
                    m.state = std::max (m.state * k, x);
                y = m.state;
                break;
            }
            case MapResponse::Accumulate:
                m.state = std::fmod (m.state + x * dt / std::max (0.05f, m.decay), 1.0f);
                y = m.state;
                break;
            case MapResponse::Toggle:
            {
                const bool above = isTriggerSource (m.source) ? trigger : x >= 0.5f;
                if (above && ! m.wasAbove) m.toggled = ! m.toggled;
                m.wasAbove = above;
                y = m.toggled ? 1.0f : 0.0f;
                break;
            }
            case MapResponse::Count: break;
        }
        m.lastInput = x;
        if (! m.enabled) { m.output = 0.0f; continue; }
        const float coef = (m.response == MapResponse::Accumulate || m.response == MapResponse::Toggle)
                         ? 1.0f : smoothCoef (dt, m.smoothing * 0.8f);
        m.smoothed += (y - m.smoothed) * coef;
        m.output = m.smoothed;

        const size_t t = (size_t) m.target;
        if (m.blend == MapBlend::Replace) { value[t] = lerpf (m.outMin, m.outMax, m.output); touched[t] = true; }
        else if (m.blend == MapBlend::Add) { add[t] += m.output * (m.outMax - m.outMin); touched[t] = true; }
    }

    // pass 2: additive offsets
    for (int i = 0; i < kNumParams; ++i) value[(size_t) i] += add[(size_t) i];

    // pass 3: Max / Min limits
    for (auto& m : maps)
    {
        if (! m.enabled || m.target < 0 || m.target >= kNumParams || ! paramInfo (m.target).isModulatable()) continue;
        const size_t t = (size_t) m.target;
        const float v = lerpf (m.outMin, m.outMax, m.output);
        if (m.blend == MapBlend::Max) { value[t] = std::max (value[t], v); touched[t] = true; }
        else if (m.blend == MapBlend::Min) { value[t] = std::min (value[t], v); touched[t] = true; }
    }

    for (int i = 0; i < kNumParams; ++i)
        if (touched[(size_t) i]) out.set ((P) i, paramInfo (i).denormalise (saturate (value[(size_t) i])));
}
} // namespace eidolon
