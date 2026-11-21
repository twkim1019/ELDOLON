#pragma once
// EIDOLON — the single parameter table.
//
// Every user-facing setting lives in one flat, versionable table. The same table drives:
//   * Visual DNA presets (entries flagged DNA) and plugin state (all entries)
//   * the generated UI panels (group / kind / range / choices / tooltip)
//   * the mapping engine (entries flagged MOD are valid mapping targets)
//   * the mutation engine (mutation group)
// Keys are persisted in JSON: never rename a key, only add new ones (state versioning relies on it).
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace eidolon
{
enum class PG : uint8_t   // UI section / panel group
{
    Layers, Piano, Trails, Constellation, Flow, Pulse, Spectrum, Orbit, Scene, Camera, Look,
    Particles, Motion, Render, Midi, Analysis, Dashboard, Capture, Count
};
enum class PK : uint8_t { Float, Int, Bool, Choice };
enum class MG : uint8_t { None, Geometry, Color, Motion, Camera, Material, Particle, Trail, Spatial, Count };
enum PF : uint8_t { DNA = 1, MOD = 2 };

// X(name, key, label, group, kind, min, max, default, choices, mutationGroup, flags, tooltip)
#define EIDOLON_PARAMS(X) \
X(Mode,            "global.mode",           "Visualization",        Layers, Choice, 0, 7, 0, "Piano Universe|Constellation|Flow|Pulse|Spectrum|Orbit|Cinematic|Custom Mix", None, DNA, "Primary visualization. Choosing a mode sets the layer mix; 'Custom Mix' keeps your own layer toggles.") \
X(PianoOn,         "layer.piano.on",        "Piano Universe",       Layers, Bool,   0, 1, 1, "", None, DNA, "Notes as 3D objects in an evolving space") \
X(PianoOpacity,    "layer.piano.opacity",   "Piano opacity",        Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(PianoScale,      "layer.piano.scale",     "Piano scale",          Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(ConstOn,         "layer.const.on",        "Constellation",        Layers, Bool,   0, 1, 0, "", None, DNA, "Notes as nodes, relationships as connections, chords as geometry") \
X(ConstOpacity,    "layer.const.opacity",   "Constellation opacity",Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(ConstScale,      "layer.const.scale",     "Constellation scale",  Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(FlowOn,          "layer.flow.on",         "Flow",                 Layers, Bool,   0, 1, 0, "", None, DNA, "Melodic trajectories per voice") \
X(FlowOpacity,     "layer.flow.opacity",    "Flow opacity",         Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(FlowScale,       "layer.flow.scale",      "Flow scale",           Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(PulseOn,         "layer.pulse.on",        "Pulse",                Layers, Bool,   0, 1, 0, "", None, DNA, "Rhythm as an energy system") \
X(PulseOpacity,    "layer.pulse.opacity",   "Pulse opacity",        Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(PulseScale,      "layer.pulse.scale",     "Pulse scale",          Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(SpecOn,          "layer.spec.on",         "Spectrum",             Layers, Bool,   0, 1, 0, "", None, DNA, "MIDI statistics (pitch / pitch class / velocity / duration distributions)") \
X(SpecOpacity,     "layer.spec.opacity",    "Spectrum opacity",     Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(SpecScale,       "layer.spec.scale",      "Spectrum scale",       Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(OrbitOn,         "layer.orbit.on",        "Orbit",                Layers, Bool,   0, 1, 0, "", None, DNA, "Circular pitch-class universe") \
X(OrbitOpacity,    "layer.orbit.opacity",   "Orbit opacity",        Layers, Float,  0, 1, 1, "", None, DNA|MOD, "Layer opacity") \
X(OrbitScale,      "layer.orbit.scale",     "Orbit scale",          Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
X(SceneOn,         "layer.scene.on",        "Scene environment",    Layers, Bool,   0, 1, 1, "", None, DNA, "Procedural cinematic environment behind the layers") \
X(SceneOpacity,    "layer.scene.opacity",   "Scene opacity",        Layers, Float,  0, 1, 0.85f, "", None, DNA|MOD, "Layer opacity") \
X(SceneScale,      "layer.scene.scale",     "Scene scale",          Layers, Float,  0.25f, 3, 1, "", Spatial, DNA|MOD, "Layer scale") \
\
X(PianoGeometry,   "piano.geometry",        "Note geometry",        Piano, Choice, 0, 10, 0, "Sphere|Cube|Ring|Crystal|Wireframe|Particle|Ribbon|Cylinder|Polygon|Torus|Abstract", Geometry, DNA|MOD, "Shape of each note object") \
X(PianoSize,       "piano.size",            "Size",                 Piano, Float, 0.2f, 3, 1, "", Geometry, DNA|MOD, "Base object size") \
X(PianoVelSize,    "piano.velSize",         "Velocity expansion",   Piano, Float, 0, 2, 1, "", Geometry, DNA|MOD, "How strongly velocity enlarges / energizes notes") \
X(PianoRotSpeed,   "piano.rotSpeed",        "Rotation speed",       Piano, Float, 0, 4, 0.6f, "", Motion, DNA|MOD, "Spin of note objects") \
X(PianoGlow,       "piano.glow",            "Glow",                 Piano, Float, 0, 1, 0.65f, "", Material, DNA|MOD, "Halo intensity") \
X(PianoEmission,   "piano.emission",        "Emission",             Piano, Float, 0, 2, 1, "", Material, DNA|MOD, "Self-illumination of note surfaces") \
X(PianoDistortion, "piano.distortion",      "Distortion",           Piano, Float, 0, 1, 0.1f, "", Geometry, DNA|MOD, "Organic deformation of geometry") \
X(PianoComplexity, "piano.complexity",      "Complexity",           Piano, Int,   3, 12, 6, "", Geometry, DNA|MOD, "Polygon sides / geometric detail") \
X(PianoRelease,    "piano.release",         "Visual release",       Piano, Float, 0.05f, 8, 1.6f, "", Motion, DNA|MOD, "Seconds a note keeps decaying after Note Off (independent of MIDI duration)") \
X(PianoTimeSpeed,  "piano.timeSpeed",       "Time flow",            Piano, Float, 0.1f, 4, 1, "", Motion, DNA|MOD, "Speed at which notes travel along the time axis") \
X(PianoPitchSpread,"piano.pitchSpread",     "Pitch spread",         Piano, Float, 0.3f, 3, 1, "", Spatial, DNA|MOD, "Vertical distance between pitches") \
X(PianoChanSpread, "piano.chanSpread",      "Channel depth",        Piano, Float, 0, 2, 0.6f, "", Spatial, DNA|MOD, "Depth separation between MIDI channels") \
X(PianoBend,       "piano.bendDeform",      "Pitch-bend deformation",Piano,Float, 0, 2, 1, "", Motion, DNA|MOD, "How pitch bend bends note trajectories") \
X(PianoParticles,  "piano.particleConvert", "Dissolve to particles",Piano, Bool,  0, 1, 1, "", Particle, DNA, "Released notes break into particles") \
X(PianoAttraction, "piano.attraction",      "Harmonic attraction",  Piano, Float, 0, 1, 0.2f, "", Spatial, DNA|MOD, "Consonant notes drift toward each other") \
X(PianoCluster,    "piano.chordCluster",    "Chord clustering",     Piano, Float, 0, 1, 0.3f, "", Spatial, DNA|MOD, "Notes of one chord gather into a cluster") \
X(PianoCollide,    "piano.collisions",      "Collisions",           Piano, Bool,  0, 1, 0, "", Motion, DNA, "Nearby notes push each other apart") \
X(PianoSpawn,      "piano.spawn",           "Spawn animation",      Piano, Float, 0, 1, 0.6f, "", Motion, DNA|MOD, "Strength of the Note On birth animation") \
X(PianoMapVel,     "piano.mapVelocity",     "Velocity maps to",     Piano, Choice, 0, 3, 0, "Size|Brightness|Energy|Size + Brightness", None, DNA, "What velocity controls") \
X(PianoMapDur,     "piano.mapDuration",     "Duration maps to",     Piano, Choice, 0, 2, 0, "Length|Lifespan|Size", None, DNA, "What note duration controls") \
X(PianoMapChan,    "piano.mapChannel",      "Channel maps to",      Piano, Choice, 0, 3, 0, "Color + Depth|Color|Depth|Geometry", None, DNA, "Visual identity of MIDI channels") \
X(PianoMapPitch,   "piano.mapPitch",        "Pitch axis",           Piano, Choice, 0, 2, 0, "Vertical|Radial|Spiral", None, DNA, "How pitch is placed in space") \
\
X(TrailOn,         "trail.on",              "Trails",               Trails, Bool,  0, 1, 1, "", Trail, DNA, "Motion trails behind notes") \
X(TrailLength,     "trail.length",          "Length",               Trails, Int,   2, 64, 24, "", Trail, DNA|MOD, "Number of trail points") \
X(TrailDensity,    "trail.density",         "Density",              Trails, Float, 0.1f, 1, 0.6f, "", Trail, DNA|MOD, "Sampling density of trail points") \
X(TrailPersist,    "trail.persistence",     "Persistence",          Trails, Float, 0, 1, 0.6f, "", Trail, DNA|MOD, "How long trails stay visible") \
X(TrailOpacity,    "trail.opacity",         "Opacity",              Trails, Float, 0, 1, 0.7f, "", Trail, DNA|MOD, "Trail opacity") \
X(TrailCurve,      "trail.curvature",       "Curvature",            Trails, Float, 0, 1, 0.5f, "", Trail, DNA|MOD, "Smoothing / curvature of trails") \
X(TrailSpeed,      "trail.speed",           "Drift speed",          Trails, Float, 0, 2, 0.4f, "", Trail, DNA|MOD, "Trails drift away from notes") \
X(TrailDecay,      "trail.decay",           "Decay",                Trails, Float, 0, 1, 0.5f, "", Trail, DNA|MOD, "Taper of trail brightness") \
X(TrailInterp,     "trail.interp",          "Interpolation",        Trails, Choice, 0, 1, 1, "Linear|Smooth", Trail, DNA, "Trail interpolation") \
X(TrailDistort,    "trail.distortion",      "Distortion",           Trails, Float, 0, 1, 0.15f, "", Trail, DNA|MOD, "Turbulent wobble of trails") \
X(TrailWidth,      "trail.width",           "Width",                Trails, Float, 0.2f, 4, 1, "", Trail, DNA|MOD, "Trail width") \
\
X(ConLayout,       "const.layout",          "Node layout",          Constellation, Choice, 0, 3, 0, "Spiral Array|Pitch Space|Circle of Fifths|Free Drift", Spatial, DNA, "Where nodes live in space") \
X(ConSimul,        "const.simultaneous",    "Link simultaneous",    Constellation, Bool, 0, 1, 1, "", None, DNA, "Connect notes sounding together") \
X(ConHarmonic,     "const.harmonic",        "Link harmonic intervals",Constellation, Bool, 0, 1, 1, "", None, DNA, "Connect consonant intervals (3rds, 5ths, 6ths)") \
X(ConPitchClass,   "const.pitchClass",      "Link same pitch class",Constellation, Bool, 0, 1, 0, "", None, DNA, "Connect equal pitch classes") \
X(ConSequential,   "const.sequential",      "Link sequential",      Constellation, Bool, 0, 1, 1, "", None, DNA, "Connect each note to the previous one") \
X(ConVoice,        "const.voice",           "Link voices",          Constellation, Bool, 0, 1, 0, "", None, DNA, "Connect consecutive notes of the same voice") \
X(ConChord,        "const.chord",           "Chord geometry",       Constellation, Bool, 0, 1, 1, "", None, DNA, "Draw recognized chords as geometric structures") \
X(ConOctave,       "const.octave",          "Link octaves",         Constellation, Bool, 0, 1, 0, "", None, DNA, "Connect octave relationships") \
X(ConCustom,       "const.custom",          "Link custom interval", Constellation, Bool, 0, 1, 0, "", None, DNA, "Connect a chosen interval") \
X(ConCustomIv,     "const.customInterval",  "Custom interval",      Constellation, Int, 1, 11, 6, "", None, DNA, "Interval (semitones) for custom links") \
X(ConDistance,     "const.distance",        "Connection distance",  Constellation, Float, 0.2f, 3, 1.4f, "", Spatial, DNA|MOD, "Maximum spatial reach of connections") \
X(ConIntensity,    "const.intensity",       "Connection intensity", Constellation, Float, 0, 2, 1, "", Material, DNA|MOD, "Brightness of connections") \
X(ConOpacity,      "const.opacity",         "Connection opacity",   Constellation, Float, 0, 1, 0.7f, "", Material, DNA|MOD, "Connection opacity") \
X(ConDuration,     "const.duration",        "Connection duration",  Constellation, Float, 0.2f, 10, 2.5f, "", Motion, DNA|MOD, "How long connections persist") \
X(ConThickness,    "const.thickness",       "Thickness",            Constellation, Float, 0.3f, 4, 1, "", Geometry, DNA|MOD, "Connection thickness") \
X(ConGeometry,     "const.geometry",        "Connection geometry",  Constellation, Choice, 0, 3, 1, "Straight|Curved|Dashed|Beam", Geometry, DNA, "Shape of connections") \
X(ConChordShape,   "const.chordShape",      "Chord shape",          Constellation, Choice, 0, 8, 0, "Polygon|Circle|Radial|Linear|Orbital|Lattice|Graph|Organic|Abstract", Geometry, DNA, "Geometric language for chords") \
X(ConNodeSize,     "const.nodeSize",        "Node size",            Constellation, Float, 0.2f, 3, 1, "", Geometry, DNA|MOD, "Node size") \
\
X(FlowVoiceMode,   "flow.voiceMode",        "Voice tracking",       Flow, Choice, 0, 5, 5, "MIDI Channel|Monophonic|Pitch Proximity|Temporal Continuity|Note Sequence|Automatic (heuristic)", None, DNA, "How notes are separated into voices. Heuristic modes can mis-assign dense polyphony.") \
X(FlowMaxVoices,   "flow.maxVoices",        "Max voices",           Flow, Int, 1, 8, 4, "", None, DNA, "Maximum tracked voices") \
X(FlowStyle,       "flow.style",            "Trajectory style",     Flow, Choice, 0, 5, 0, "Ribbon|Line|Curve|Particles|Fluid|Geometric", Geometry, DNA, "How trajectories are drawn") \
X(FlowWidth,       "flow.width",            "Width",                Flow, Float, 0.2f, 4, 1, "", Geometry, DNA|MOD, "Trajectory width") \
X(FlowHistory,     "flow.history",          "History",              Flow, Float, 2, 30, 10, "", Trail, DNA|MOD, "Seconds of melody kept visible") \
X(FlowSmooth,      "flow.smoothing",        "Smoothing",            Flow, Float, 0, 1, 0.6f, "", Trail, DNA|MOD, "Curve smoothing between notes") \
X(FlowLeap,        "flow.leapEmphasis",     "Leap emphasis",        Flow, Float, 0, 2, 1, "", Material, DNA|MOD, "Highlight large melodic leaps") \
X(FlowSpeed,       "flow.speed",            "Speed",                Flow, Float, 0.1f, 4, 1, "", Motion, DNA|MOD, "Scroll speed of trajectories") \
X(FlowVelWidth,    "flow.velocityWidth",    "Velocity width",       Flow, Float, 0, 2, 1, "", Geometry, DNA|MOD, "Velocity modulates width") \
\
X(PulseTrigger,    "pulse.trigger",         "Respond to",           Pulse, Choice, 0, 8, 0, "Every Note On|Note Off|Velocity Threshold|Onset Density|Beat|Bar|Polyphony|CC|Chord Change", None, DNA, "Event that launches pulses") \
X(PulseVelThresh,  "pulse.velThreshold",    "Velocity threshold",   Pulse, Int, 1, 127, 80, "", None, DNA, "Minimum velocity for the threshold trigger") \
X(PulseCC,         "pulse.cc",              "Trigger CC",           Pulse, Int, 0, 127, 1, "", None, DNA, "Controller for the CC trigger") \
X(PulseStyle,      "pulse.style",           "Style",                Pulse, Choice, 0, 5, 0, "Luminous Rings|Expanding Waves|Particle Bursts|Oscillating Geometry|Radial Rays|Breathing Core", Geometry, DNA, "Visual form of pulses") \
X(PulseStrength,   "pulse.strength",        "Strength",             Pulse, Float, 0, 2, 1, "", Material, DNA|MOD, "Pulse brightness") \
X(PulseDecay,      "pulse.decay",           "Decay",                Pulse, Float, 0.1f, 4, 1.2f, "", Motion, DNA|MOD, "Pulse decay time") \
X(PulseRadius,     "pulse.radius",          "Radius",               Pulse, Float, 0.2f, 3, 1, "", Spatial, DNA|MOD, "Maximum pulse radius") \
X(PulseExpansion,  "pulse.expansion",       "Expansion",            Pulse, Float, 0.1f, 3, 1, "", Motion, DNA|MOD, "Expansion speed") \
X(PulseDeform,     "pulse.deformation",     "Deformation",          Pulse, Float, 0, 1, 0.3f, "", Geometry, DNA|MOD, "Wave-like deformation of rings") \
X(PulseFreq,       "pulse.frequency",       "Oscillation",          Pulse, Float, 0, 4, 1, "", Motion, DNA|MOD, "Oscillation frequency of geometry") \
X(PulseSync,       "pulse.sync",            "Beat sync",            Pulse, Bool, 0, 1, 1, "", None, DNA, "Lock oscillations to beat / bar when tempo is known") \
X(PulseVariation,  "pulse.variation",       "Variation",            Pulse, Float, 0, 1, 0.5f, "", Motion, DNA|MOD, "Randomizes pulses so they never repeat mechanically") \
\
X(SpecView,        "spec.view",             "View",                 Spectrum, Choice, 0, 7, 1, "Pitch Histogram|Pitch-Class Radial|Octave Distribution|Velocity Distribution|Duration Distribution|Pitch-Class Heatmap|Polyphony Timeline|Density Timeline", None, DNA, "Statistic shown") \
X(SpecWindow,      "spec.window",           "Analysis window",      Spectrum, Choice, 0, 6, 2, "Instant|1 s|5 s|10 s|30 s|Entire capture|Custom", None, DNA, "Time window of the statistics") \
X(SpecCustom,      "spec.customWindow",     "Custom window (s)",    Spectrum, Float, 1, 300, 20, "", None, DNA, "Custom analysis window") \
X(SpecSmooth,      "spec.smoothing",        "Transition smoothing", Spectrum, Float, 0, 1, 0.6f, "", Motion, DNA|MOD, "Smoothness of statistical transitions") \
X(SpecPlacement,   "spec.placement",        "Placement",            Spectrum, Choice, 0, 2, 0, "Floor (3D)|Panel|Fullscreen", Spatial, DNA, "Where the statistics are drawn") \
X(SpecGlow,        "spec.glow",             "Glow",                 Spectrum, Float, 0, 1, 0.6f, "", Material, DNA|MOD, "Bar glow") \
\
X(OrbMode,         "orbit.mode",            "Orbit mode",           Orbit, Choice, 0, 7, 1, "Pitch|Pitch Class|Octave|Chord|Velocity|Duration|Voice|Harmonic Relationship", None, DNA, "What drives the orbits") \
X(OrbFifths,       "orbit.fifths",          "Circle of fifths",     Orbit, Bool, 0, 1, 0, "", Spatial, DNA, "Arrange pitch classes by fifths instead of chromatically") \
X(OrbRadius,       "orbit.radius",          "Radius",               Orbit, Float, 0.3f, 3, 1, "", Spatial, DNA|MOD, "Orbit radius") \
X(OrbRotation,     "orbit.rotation",        "Rotation",             Orbit, Float, 0, 360, 0, "", Spatial, DNA|MOD, "Base rotation (degrees)") \
X(OrbRotSpeed,     "orbit.rotSpeed",        "Rotation speed",       Orbit, Float, -2, 2, 0.15f, "", Motion, DNA|MOD, "Rotation speed of the whole system") \
X(OrbNodeSize,     "orbit.nodeSize",        "Node size",            Orbit, Float, 0.2f, 3, 1, "", Geometry, DNA|MOD, "Size of orbiting notes") \
X(OrbTrail,        "orbit.trail",           "Orbit trails",         Orbit, Bool, 0, 1, 1, "", Trail, DNA, "Arcs behind orbiting notes") \
X(OrbConnect,      "orbit.connect",         "Chord connections",    Orbit, Bool, 0, 1, 1, "", None, DNA, "Connect simultaneous notes inside the circle") \
X(OrbRings,        "orbit.rings",           "Octave rings",         Orbit, Bool, 0, 1, 1, "", None, DNA, "Show octave rings") \
X(OrbGeometry,     "orbit.geometry",        "Node geometry",        Orbit, Choice, 0, 3, 0, "Glow|Ring|Crystal|Polygon", Geometry, DNA, "Shape of orbiting notes") \
\
X(SceneCurrent,    "scene.current",         "Scene",                Scene, Choice, 0, 9, 0, "Nebula|Cathedral|Aurora|Ocean|Celestial|Crystal|Void|Bloom|Fractal|Lattice", None, DNA, "Active procedural scene") \
X(SceneAuto,       "scene.auto",            "Auto transitions",     Scene, Bool, 0, 1, 1, "", None, DNA, "Let musical activity move between scenes") \
X(SceneTrigger,    "scene.trigger",         "Transition trigger",   Scene, Choice, 0, 11, 3, "MIDI Density|Polyphony|Average Velocity|Chord Change|Register Change|Onset Rate|CC|Beat|Bar|Pitch Class|Note Count|Manual only", None, DNA, "Musical event that advances scenes") \
X(SceneThreshold,  "scene.threshold",       "Threshold",            Scene, Float, 0, 1, 0.55f, "", None, DNA, "Trigger threshold (normalized)") \
X(SceneCC,         "scene.cc",              "Trigger CC",           Scene, Int, 0, 127, 1, "", None, DNA, "Controller for the CC trigger") \
X(SceneTransType,  "scene.transition",      "Transition type",      Scene, Choice, 0, 7, 2, "Crossfade|Dissolve|Morph|Expansion|Collapse|Transformation|Particle|Geometry", None, DNA, "How scenes transform into each other") \
X(SceneTransTime,  "scene.transitionTime",  "Transition duration",  Scene, Float, 0.2f, 12, 3.5f, "", Motion, DNA|MOD, "Seconds per transition") \
X(SceneIntensity,  "scene.intensity",       "Intensity",            Scene, Float, 0, 2, 1, "", Material, DNA|MOD, "Scene brightness / activity") \
X(SceneSensitivity,"scene.sensitivity",     "Response sensitivity", Scene, Float, 0, 2, 1, "", None, DNA|MOD, "How strongly scenes respond to the music") \
X(SceneBlend,      "scene.blend",           "Scene blending",       Scene, Float, 0, 1, 0, "", None, DNA|MOD, "Keeps part of the previous scene blended in") \
X(SceneMinHold,    "scene.minHold",         "Minimum hold (s)",     Scene, Float, 1, 120, 12, "", None, DNA, "Shortest time between automatic transitions") \
X(SceneOrder,      "scene.order",           "Next scene",           Scene, Choice, 0, 2, 2, "Sequential|Random|Energy-matched", None, DNA, "How the next scene is chosen") \
X(SceneAdvance,    "scene.advance",         "Advance (momentary)",  Scene, Float, 0, 1, 0, "", None, MOD, "Mapping target: crossing 0.5 upward triggers the next scene") \
\
X(CamMode,         "cam.mode",              "Camera mode",          Camera, Choice, 0, 6, 6, "Static|Orbit|Follow|Follow Active Notes|Follow Musical Center|Cinematic|Automatic", Camera, DNA, "Camera behavior") \
X(CamFov,          "cam.fov",               "Field of view",        Camera, Float, 25, 100, 55, "", Camera, DNA|MOD, "Field of view (degrees)") \
X(CamDistance,     "cam.distance",          "Distance",             Camera, Float, 3, 40, 14, "", Camera, DNA|MOD, "Camera distance") \
X(CamYaw,          "cam.yaw",               "Yaw",                  Camera, Float, -180, 180, -28, "", Camera, DNA|MOD, "Horizontal angle") \
X(CamPitch,        "cam.pitch",             "Pitch",                Camera, Float, -80, 80, 12, "", Camera, DNA|MOD, "Vertical angle") \
X(CamSpeed,        "cam.speed",             "Camera speed",         Camera, Float, 0, 3, 0.6f, "", Camera, DNA|MOD, "Orbit / cinematic motion speed") \
X(CamSmooth,       "cam.smoothness",        "Smoothness",           Camera, Float, 0, 1, 0.75f, "", Camera, DNA|MOD, "Damping of camera motion") \
X(CamReact,        "cam.react",             "Musical response",     Camera, Float, 0, 1, 0.4f, "", Camera, DNA|MOD, "Camera responds to pitch, velocity, density and chords") \
X(CamShake,        "cam.shake",             "Impact",               Camera, Float, 0, 1, 0.15f, "", Camera, DNA|MOD, "Velocity-driven camera impulse") \
\
X(LookPalette,     "look.palette",          "Palette",              Look, Choice, 0, 8, 0, "Aurora|Ember|Glacier|Monochrome|Spectral|Nocturne|Solar|Verdant|Rose Quartz", Color, DNA, "Color palette") \
X(LookColorBy,     "look.colorBy",          "Color by",             Look, Choice, 0, 5, 0, "Pitch Class|Channel|Velocity|Octave|Voice|Chord", Color, DNA, "Musical property that selects colors") \
X(LookHue,         "look.hueShift",         "Hue shift",            Look, Float, 0, 1, 0, "", Color, DNA|MOD, "Rotate the palette") \
X(LookSaturation,  "look.saturation",       "Saturation",           Look, Float, 0, 1.5f, 1, "", Color, DNA|MOD, "Color saturation") \
X(LookBrightness,  "look.brightness",       "Brightness",           Look, Float, 0.2f, 2, 1, "", Color, DNA|MOD, "Overall brightness") \
X(LookBloom,       "look.bloom",            "Bloom",                Look, Float, 0, 1, 0.6f, "", Material, DNA|MOD, "Glow spread") \
X(LookMaterial,    "look.material",         "Material",             Look, Choice, 0, 5, 0, "Luminous|Glass|Matte|Metallic|Plasma|Hologram", Material, DNA|MOD, "Surface appearance of geometry") \
X(LookLightAngle,  "look.lightAngle",       "Light angle",          Look, Float, 0, 360, 40, "", Material, DNA|MOD, "Key light direction") \
X(LookAmbient,     "look.ambient",          "Ambient light",        Look, Float, 0, 1, 0.35f, "", Material, DNA|MOD, "Ambient light level") \
X(LookBackground,  "look.background",       "Background tint",      Look, Float, 0, 1, 0.3f, "", Color, DNA|MOD, "Background color depth") \
X(LookVignette,    "look.vignette",         "Vignette",             Look, Float, 0, 1, 0.55f, "", Color, DNA|MOD, "Edge darkening") \
X(LookTexture,     "look.texture",          "Texture",              Look, Float, 0, 1, 0.2f, "", Material, DNA|MOD, "Fine surface texture / film grain") \
\
X(PartOn,          "part.on",               "Particles",            Particles, Bool, 0, 1, 1, "", Particle, DNA, "Particle system") \
X(PartDensity,     "part.density",          "Density",              Particles, Float, 0, 2, 1, "", Particle, DNA|MOD, "Particle emission density") \
X(PartSpeed,       "part.speed",            "Speed",                Particles, Float, 0, 3, 1, "", Particle, DNA|MOD, "Particle speed") \
X(PartSize,        "part.size",             "Size",                 Particles, Float, 0.2f, 3, 1, "", Particle, DNA|MOD, "Particle size") \
X(PartLife,        "part.life",             "Lifetime",             Particles, Float, 0.2f, 8, 2.2f, "", Particle, DNA|MOD, "Particle lifetime (s)") \
X(PartGravity,     "part.gravity",          "Gravity",              Particles, Float, -1, 1, -0.1f, "", Particle, DNA|MOD, "Negative values rise") \
X(PartTurbulence,  "part.turbulence",       "Turbulence",           Particles, Float, 0, 2, 0.6f, "", Particle, DNA|MOD, "Swirling motion") \
X(PartBurst,       "part.burst",            "Note bursts",          Particles, Float, 0, 2, 0.8f, "", Particle, DNA|MOD, "Particles emitted on Note On") \
\
X(MotionSpeed,     "motion.speed",          "Global time scale",    Motion, Float, 0.1f, 3, 1, "", Motion, DNA|MOD, "Speed of all procedural animation") \
X(MotionDrift,     "motion.drift",          "Drift",                Motion, Float, 0, 1, 0.3f, "", Motion, DNA|MOD, "Slow organic drift of objects") \
X(MotionEnergy,    "motion.energy",         "Visual energy",        Motion, Float, 0, 2, 1, "", Motion, DNA|MOD, "Global energy multiplier (great mapping target)") \
X(MotionSeedUse,   "motion.seedVariation",  "Seed variation",       Motion, Float, 0, 1, 0.5f, "", Motion, DNA, "How much the random seed varies procedural details") \
\
X(RenderQuality,   "render.quality",        "Quality preset",       Render, Choice, 0, 3, 2, "Low|Medium|High|Ultra", None, DNA, "Sets particle limit, trail quality, geometry detail and motion quality") \
X(RenderFps,       "render.fps",            "FPS target",           Render, Int, 15, 120, 60, "", None, DNA, "Target frame rate") \
X(RenderParticles, "render.particleLimit",  "Particle limit",       Render, Int, 200, 20000, 6000, "", None, DNA, "Maximum live particles") \
X(RenderTrailQ,    "render.trailQuality",   "Trail quality",        Render, Float, 0, 1, 0.8f, "", None, DNA, "Trail point budget") \
X(RenderGeomQ,     "render.geometryDetail", "Geometry complexity",  Render, Float, 0, 1, 0.8f, "", None, DNA, "Geometric detail budget") \
X(RenderAA,        "render.antialias",      "Anti-aliasing",        Render, Bool, 0, 1, 1, "", None, DNA, "Multisampling on the GPU path, high-quality resampling on the CPU path") \
X(RenderMotionQ,   "render.motionQuality",  "Motion quality",       Render, Float, 0, 1, 0.8f, "", None, DNA, "Simulation sub-steps and interpolation") \
X(RenderAdaptive,  "render.adaptive",       "Adaptive quality",     Render, Bool, 0, 1, 1, "", None, DNA, "Automatically lowers detail when frames take too long") \
X(RenderGpu,       "render.gpu",            "GPU acceleration",     Render, Bool, 0, 1, 1, "", None, 0, "OpenGL-accelerated canvas (falls back to CPU automatically)") \
X(RenderPerf,      "render.perfOverlay",    "Performance overlay",  Render, Bool, 0, 1, 0, "", None, 0, "Show frame time, FPS and object counts") \
\
X(MidiChannel,     "midi.channel",          "MIDI channel filter",  Midi, Choice, 0, 16, 0, "All|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16", None, 0, "Only visualize this channel") \
X(MidiThru,        "midi.thru",             "MIDI thru",            Midi, Bool, 0, 1, 1, "", None, 0, "Pass incoming MIDI to the plugin's MIDI output") \
X(MidiStuck,       "midi.stuckTimeout",     "Stuck-note timeout (s)",Midi, Float, 0, 300, 90, "", None, 0, "Release notes held longer than this (0 = never)") \
X(MidiSustain,     "midi.sustainAware",     "Sustain-aware",        Midi, Bool, 0, 1, 1, "", None, 0, "Keys released under the sustain pedal stay alive until the pedal lifts") \
\
X(AnaChordHold,    "ana.chordHold",         "Chord stabilization (s)",Analysis, Float, 0, 1, 0.09f, "", None, 0, "A new chord must persist this long before it is shown (prevents flicker)") \
X(AnaChordRelease, "ana.chordRelease",      "Chord release hold (s)",Analysis, Float, 0, 3, 0.4f, "", None, 0, "Keep the last chord this long after notes stop") \
X(AnaChordMinNotes,"ana.chordMinNotes",     "Min chord notes",      Analysis, Int, 2, 4, 3, "", None, 0, "Distinct pitch classes needed before a chord is reported") \
X(AnaChordMinConf, "ana.chordMinConfidence","Min confidence",       Analysis, Float, 0, 1, 0.45f, "", None, 0, "Below this the collection is reported as unrecognized, not forced into a chord") \
X(AnaChordSustain, "ana.chordUseSustain",   "Include sustained notes",Analysis, Bool, 0, 1, 1, "", None, 0, "Sustained (pedal-held) notes count toward chords") \
X(AnaCustomChord,  "ana.customChord",       "Custom chord template",Analysis, Int, 0, 4095, 0, "", None, 0, "12-bit interval mask from the root (bit 0 = root)") \
X(AnaScaleWindow,  "ana.scaleWindow",       "Scale memory (s)",     Analysis, Float, 2, 120, 20, "", None, 0, "Time constant of the scale estimator") \
X(AnaCustomScale,  "ana.customScale",       "Custom scale",         Analysis, Int, 0, 4095, 0, "", None, 0, "12-bit pitch-class mask from the root (bit 0 = root)") \
X(AnaVoiceGap,     "ana.voiceGap",          "Voice continuity (s)", Analysis, Float, 0.1f, 4, 1, "", None, 0, "Silence after which a voice may be reused freely") \
\
X(DashOn,          "dash.on",               "Dashboard",            Dashboard, Bool, 0, 1, 1, "", None, 0, "Live musical analysis overlay") \
X(DashPos,         "dash.position",         "Position",             Dashboard, Choice, 0, 3, 0, "Top Left|Top Right|Bottom Left|Bottom Right", None, 0, "Dashboard corner") \
X(DashStyle,       "dash.style",            "Display style",        Dashboard, Choice, 0, 2, 0, "Detailed|Compact|Minimal", None, 0, "Dashboard density") \
X(DashSize,        "dash.size",             "Panel size",           Dashboard, Float, 0.7f, 1.6f, 1, "", None, 0, "Dashboard scale") \
X(DashRate,        "dash.rate",             "Update frequency (Hz)",Dashboard, Int, 2, 60, 20, "", None, 0, "How often values refresh") \
X(DashSmooth,      "dash.smoothing",        "Data smoothing",       Dashboard, Float, 0, 1, 0.5f, "", None, 0, "Smoothing of numeric readouts") \
X(DashChord,       "dash.chord",            "Chord panel",          Dashboard, Bool, 0, 1, 1, "", None, 0, "Chord, confidence, inversion, transition") \
X(DashScale,       "dash.scale",            "Scale panel",          Dashboard, Bool, 0, 1, 1, "", None, 0, "Scale estimate") \
X(DashNotes,       "dash.notes",            "Notes panel",          Dashboard, Bool, 0, 1, 1, "", None, 0, "Active notes, polyphony, range, velocity") \
X(DashRhythm,      "dash.rhythm",           "Rhythm panel",         Dashboard, Bool, 0, 1, 1, "", None, 0, "Density, onset rate, tempo, contour") \
X(DashChannels,    "dash.channels",         "Channels panel",       Dashboard, Bool, 0, 1, 0, "", None, 0, "Per-channel activity") \
X(DashControllers, "dash.controllers",      "Controllers panel",    Dashboard, Bool, 0, 1, 0, "", None, 0, "CC, pitch bend, pressure, sustain") \
\
X(CapMode,         "cap.mode",              "Capture mode",         Capture, Choice, 0, 4, 0, "Live Buffer|Fixed Duration|Triggered|Phrase|Loop", None, 0, "How captures start and stop") \
X(CapDuration,     "cap.duration",          "Duration",             Capture, Choice, 0, 4, 2, "5 s|15 s|30 s|60 s|Custom", None, 0, "Capture length") \
X(CapCustom,       "cap.custom",            "Custom duration (s)",  Capture, Float, 1, 600, 120, "", None, 0, "Custom capture length") \
X(CapPhraseGap,    "cap.phraseGap",         "Phrase end gap (s)",   Capture, Float, 0.3f, 10, 1.5f, "", None, 0, "Silence that ends a phrase capture") \
X(CapReplayLoop,   "cap.replayLoop",        "Loop replay",          Capture, Bool, 0, 1, 0, "", None, 0, "Repeat the replayed capture")

enum class P : int
{
   #define EIDOLON_ENUM(name, ...) name,
    EIDOLON_PARAMS (EIDOLON_ENUM)
   #undef EIDOLON_ENUM
    Count
};
constexpr int kNumParams = (int) P::Count;

struct ParamInfo
{
    P id;
    const char* key;
    const char* label;
    PG group;
    PK kind;
    float minV, maxV, defV;
    const char* choices;   // '|' separated (Choice only)
    MG mutation;
    uint8_t flags;
    const char* tooltip;

    bool isDNA() const { return (flags & DNA) != 0; }
    bool isModulatable() const { return (flags & MOD) != 0; }
    int numChoices() const;
    std::string choiceName (int index) const;
    float normalise (float v) const   { return maxV > minV ? (v - minV) / (maxV - minV) : 0.0f; }
    float denormalise (float n) const { return minV + n * (maxV - minV); }
    float sanitise (float v) const;   // clamp + quantize ints / bools / choices
};

const ParamInfo& paramInfo (P p);
inline const ParamInfo& paramInfo (int i) { return paramInfo ((P) i); }
int findParamByKey (std::string_view key);   // -1 if unknown
const char* groupName (PG g);
const char* mutationGroupName (MG m);

/** A full set of parameter values (one float per table entry). */
struct ParamSet
{
    std::array<float, kNumParams> v {};

    ParamSet() { resetToDefaults(); }
    void resetToDefaults();
    float get (P p) const       { return v[(size_t) p]; }
    int geti (P p) const        { return (int) std::lround (v[(size_t) p]); }
    bool getb (P p) const       { return v[(size_t) p] >= 0.5f; }
    void set (P p, float value) { v[(size_t) p] = paramInfo (p).sanitise (value); }
    float norm (P p) const      { return paramInfo (p).normalise (v[(size_t) p]); }
    bool operator== (const ParamSet& o) const { return v == o.v; }
};
} // namespace eidolon
