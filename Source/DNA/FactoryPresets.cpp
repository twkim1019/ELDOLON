#include "FactoryPresets.h"
#include <functional>

namespace eidolon
{
namespace
{
Mapping map (MapSource src, P target, float lo, float hi, MapBlend blend, MapResponse resp = MapResponse::Direct, int cc = 1)
{
    Mapping m;
    m.source = src; m.target = (int) target; m.outMin = lo; m.outMax = hi; m.blend = blend; m.response = resp; m.ccNumber = cc;
    if (resp == MapResponse::Envelope) { m.decay = 0.45f; m.smoothing = 0.05f; }
    return m;
}

VisualDNA make (const char* name, const char* category, uint32_t seed, int mode, int scene,
                const std::function<void (ParamSet&)>& tweak, std::vector<Mapping> maps = {})
{
    VisualDNA d;
    d.name = name; d.category = category; d.author = "EIDOLON"; d.seed = seed;
    applyModeLayers (d.params, mode);
    d.params.set (P::SceneCurrent, (float) scene);
    if (tweak) tweak (d.params);
    d.mappings = std::move (maps);
    return d;
}

std::vector<Mapping> standardMaps()
{
    return { map (MapSource::CC, P::MotionEnergy, 0.0f, 0.45f, MapBlend::Add),                     // mod wheel -> energy
             map (MapSource::Sustain, P::TrailPersist, 0.0f, 0.92f, MapBlend::Max),               // pedal -> long trails
             map (MapSource::NoteOn, P::LookBloom, 0.0f, 0.25f, MapBlend::Add, MapResponse::Envelope) };
}
} // namespace

VisualDNA defaultDNA() { return factoryPresets().front(); }

const std::vector<VisualDNA>& factoryPresets()
{
    static const std::vector<VisualDNA> presets = []
    {
        std::vector<VisualDNA> v;
        v.push_back (make ("First Light", "Cinematic", 1, 0, 0, [] (ParamSet&) {}, standardMaps()));
        v.push_back (make ("Glass Cathedral", "Geometric", 11, 1, 1, [] (ParamSet& p)
        {
            p.set (P::PianoOn, 1); p.set (P::PianoOpacity, 0.55f); p.set (P::PianoGeometry, 3); p.set (P::LookPalette, 2);
            p.set (P::LookMaterial, 1); p.set (P::ConChordShape, 0); p.set (P::CamMode, 1); p.set (P::CamSpeed, 0.25f);
        }, standardMaps()));
        v.push_back (make ("Aurora Lines", "Organic", 23, 2, 2, [] (ParamSet& p)
        {
            p.set (P::FlowStyle, 0); p.set (P::FlowWidth, 1.6f); p.set (P::LookPalette, 0); p.set (P::FlowSmooth, 0.8f);
            p.set (P::CamMode, 5);
        }, { map (MapSource::PitchBend, P::FlowWidth, 0.2f, 0.9f, MapBlend::Replace), map (MapSource::CC, P::LookHue, 0.0f, 0.5f, MapBlend::Add) }));
        v.push_back (make ("Ocean Pulse", "Organic", 37, 3, 3, [] (ParamSet& p)
        {
            p.set (P::PulseStyle, 1); p.set (P::LookPalette, 2); p.set (P::PianoOn, 1); p.set (P::PianoOpacity, 0.5f);
            p.set (P::PianoGeometry, 2); p.set (P::Mode, 7);
        }, standardMaps()));
        v.push_back (make ("Celestial Mechanics", "Celestial", 41, 5, 4, [] (ParamSet& p)
        {
            p.set (P::OrbFifths, 1); p.set (P::LookPalette, 5); p.set (P::OrbRotSpeed, 0.08f); p.set (P::CamPitch, 38);
            p.set (P::CamMode, 1); p.set (P::CamSpeed, 0.15f);
        }, standardMaps()));
        v.push_back (make ("Crystal Harmony", "Geometric", 53, 0, 5, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 3); p.set (P::ConstOn, 1); p.set (P::ConstOpacity, 0.7f); p.set (P::LookMaterial, 1);
            p.set (P::LookPalette, 2); p.set (P::Mode, 7); p.set (P::PianoCluster, 0.7f);
        }, standardMaps()));
        v.push_back (make ("Void Monk", "Minimal", 61, 0, 6, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 4); p.set (P::LookPalette, 3); p.set (P::PartDensity, 0.2f); p.set (P::PianoGlow, 0.3f);
            p.set (P::TrailOpacity, 0.35f); p.set (P::LookBloom, 0.25f); p.set (P::CamMode, 0); p.set (P::MotionSpeed, 0.6f);
        }));
        v.push_back (make ("Bloom Garden", "Organic", 71, 3, 7, [] (ParamSet& p)
        {
            p.set (P::PulseStyle, 2); p.set (P::PartDensity, 1.6f); p.set (P::PartGravity, -0.4f); p.set (P::LookPalette, 7);
            p.set (P::PianoOn, 1); p.set (P::PianoGeometry, 5); p.set (P::Mode, 7);
        }, standardMaps()));
        v.push_back (make ("Fractal Choir", "Abstract", 83, 1, 8, [] (ParamSet& p)
        {
            p.set (P::ConChordShape, 7); p.set (P::ConLayout, 0); p.set (P::LookPalette, 4); p.set (P::CamMode, 5);
        }, standardMaps()));
        v.push_back (make ("Lattice Mind", "Geometric", 97, 1, 9, [] (ParamSet& p)
        {
            p.set (P::ConChordShape, 5); p.set (P::ConLayout, 1); p.set (P::ConPitchClass, 1); p.set (P::ConOctave, 1);
            p.set (P::LookPalette, 2); p.set (P::ConGeometry, 0);
        }, standardMaps()));
        v.push_back (make ("Spectral Analyst", "Minimal", 101, 4, 6, [] (ParamSet& p)
        {
            p.set (P::SpecView, 1); p.set (P::SpecWindow, 3); p.set (P::LookPalette, 4); p.set (P::SceneOpacity, 0.5f);
        }));
        v.push_back (make ("Ember Rain", "Abstract", 113, 0, 0, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 5); p.set (P::LookPalette, 1); p.set (P::PartGravity, 0.45f); p.set (P::PartDensity, 1.4f);
            p.set (P::PianoRelease, 3.5f);
        }, standardMaps()));
        v.push_back (make ("Torus Field", "Experimental", 127, 0, 8, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 9); p.set (P::LookMaterial, 4); p.set (P::PianoDistortion, 0.55f); p.set (P::LookPalette, 4);
            p.set (P::PianoRotSpeed, 1.8f);
        }, { map (MapSource::CC, P::PianoDistortion, 0.0f, 1.0f, MapBlend::Replace),
             map (MapSource::NoteDensity, P::PianoRotSpeed, 0.1f, 0.9f, MapBlend::Max) }));
        v.push_back (make ("Cinematic Auto", "Cinematic", 131, 6, 0, [] (ParamSet& p)
        {
            p.set (P::SceneAuto, 1); p.set (P::SceneTrigger, 0); p.set (P::SceneMinHold, 16); p.set (P::SceneTransType, 2);
            p.set (P::CamMode, 6);
        }, standardMaps()));
        v.push_back (make ("Starfield Ribbons", "Celestial", 139, 0, 4, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 6); p.set (P::LookPalette, 5); p.set (P::TrailLength, 48); p.set (P::PianoMapDur, 0);
        }, standardMaps()));
        v.push_back (make ("Neon Monolith", "Experimental", 149, 0, 9, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 1); p.set (P::LookMaterial, 5); p.set (P::LookPalette, 8); p.set (P::PianoSize, 1.4f);
            p.set (P::PianoCollide, 1);
        }, standardMaps()));
        v.push_back (make ("Quiet Nocturne", "Minimal", 157, 0, 6, [] (ParamSet& p)
        {
            p.set (P::PianoGeometry, 2); p.set (P::LookPalette, 5); p.set (P::MotionSpeed, 0.5f); p.set (P::PianoTimeSpeed, 0.5f);
            p.set (P::PartDensity, 0.4f); p.set (P::LookBrightness, 0.8f);
        }));
        v.push_back (make ("Polyphonic Rivers", "Organic", 163, 2, 3, [] (ParamSet& p)
        {
            p.set (P::FlowMaxVoices, 6); p.set (P::FlowStyle, 4); p.set (P::LookColorBy, 4); p.set (P::LookPalette, 2);
        }, standardMaps()));
        v.push_back (make ("Harmonic Graph", "Abstract", 173, 1, 9, [] (ParamSet& p)
        {
            p.set (P::ConChordShape, 6); p.set (P::ConPitchClass, 1); p.set (P::ConVoice, 1); p.set (P::ConCustom, 1);
            p.set (P::ConGeometry, 3); p.set (P::LookPalette, 6);
        }));
        v.push_back (make ("Solar Choir", "Cinematic", 181, 7, 7, [] (ParamSet& p)
        {
            p.set (P::OrbitOn, 1); p.set (P::PulseOn, 1); p.set (P::PianoOn, 0); p.set (P::LookPalette, 6);
            p.set (P::OrbitOpacity, 0.8f); p.set (P::PulseOpacity, 0.6f); p.set (P::Mode, 7);
        }, standardMaps()));
        return v;
    }();
    return presets;
}
} // namespace eidolon
