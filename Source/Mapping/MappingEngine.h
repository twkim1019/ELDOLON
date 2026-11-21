#pragma once
// EIDOLON — MIDI-to-visual mapping engine.
//
// Each mapping: source -> input range -> dead zone -> sensitivity -> inversion -> curve -> response
//               -> smoothing -> output range (normalized target range).
//
// Arbitration when several mappings drive the same target (applied in this order):
//   1. Replace : the LAST enabled Replace mapping in the list sets the value (list order = priority)
//   2. Add     : every Add mapping adds  output * (max - min)  (a negative range subtracts)
//   3. Max/Min : the value is limited to at least / at most each Max / Min mapping's value
//   The result is clamped to the parameter's range and quantized for integer / choice targets.
#include "../Core/Params.h"
#include <array>
#include <string>
#include <vector>

namespace eidolon
{
enum class MapSource : int
{
    NoteOn, NoteOff, Velocity, Pitch, Duration, Channel, CC, PitchBend, ChannelPressure, PolyAftertouch,
    Sustain, Beat, Bar, Polyphony, NoteDensity, ChordChange, Count
};
enum class MapCurve : int { Linear, Exponential, Logarithmic, SCurve, Custom, Count };
enum class MapResponse : int { Direct, Envelope, Accumulate, Toggle, Count };
enum class MapBlend : int { Replace, Add, Max, Min, Count };

const char* mapSourceName (MapSource s);
const char* mapCurveName (MapCurve c);
const char* mapResponseName (MapResponse r);
const char* mapBlendName (MapBlend b);

struct Mapping
{
    bool enabled = true;
    MapSource source = MapSource::Velocity;
    int ccNumber = 1;
    int channel = 0;                  // 0 = any, 1..16
    int target = (int) P::PianoSize;  // parameter index (must be a MOD parameter)
    float outMin = 0.0f, outMax = 1.0f;
    float inMin = 0.0f, inMax = 1.0f; // input range
    float deadZone = 0.0f;
    float sensitivity = 1.0f;
    bool invert = false;
    MapCurve curve = MapCurve::Linear;
    float curveAmount = 0.5f;
    std::array<float, 3> customPoints { 0.25f, 0.5f, 0.75f };   // custom curve: y at x = 0.25 / 0.5 / 0.75
    MapResponse response = MapResponse::Direct;
    float decay = 0.5f;               // envelope decay (s) / accumulate rate
    float smoothing = 0.15f;          // 0..1  (time constant up to 0.8 s)
    MapBlend blend = MapBlend::Replace;

    // runtime (not persisted)
    float state = 0.0f, smoothed = 0.0f, lastInput = 0.0f, output = 0.0f;
    bool toggled = false, wasAbove = false;

    void resetRuntime() { state = smoothed = lastInput = output = 0.0f; toggled = wasAbove = false; }
};

/** Per-frame MIDI state consumed by the mapping engine. */
struct MappingInputs
{
    std::array<int, 16> noteOns {}, noteOffs {};
    std::array<float, 16> lastVelocity {}, lastPitch {};
    float lastVelocityAny = 0.0f, lastPitchAny = 0.0f, lastChannel = 0.0f;
    float averageDuration = 0.0f;                     // held duration of sounding notes (s)
    std::array<std::array<float, 128>, 16> cc {};     // 0..1
    std::array<float, 16> bend {}, pressure {}, polyAT {};
    std::array<bool, 16> sustain {};
    float beatPhase = 0.0f, barPhase = 0.0f;
    bool beatTick = false, barTick = false, chordChanged = false;
    int polyphony = 0;
    float density = 0.0f;
};

float applyCurve (const Mapping& m, float x);

class MappingEngine
{
public:
    /** Evaluates `maps` (updating their runtime state) and writes base + modulation into `out`. */
    static void process (std::vector<Mapping>& maps, const MappingInputs& in, float dt, const ParamSet& base, ParamSet& out);
    static float rawSourceValue (const Mapping& m, const MappingInputs& in, bool& trigger);
    static bool isTriggerSource (MapSource s);
};
} // namespace eidolon
