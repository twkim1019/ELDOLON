# EIDOLON — User Guide

## Window layout

| Area | What it shows |
|---|---|
| **Top bar** | EIDOLON mark · MIDI status pill (NO MIDI YET / MIDI ACTIVE / MIDI IDLE — driven only by received events) · visualization mode · preset browser with ◀ ▶ · undo / redo · renderer status (GPU / CPU and FPS) · focus mode · pop-out window · side-panel toggle · render settings |
| **Canvas** | The visualization. Drag = orbit, right-drag or Shift-drag = pan, wheel = zoom, double-click = reset camera. The musical dashboard floats in one corner. |
| **Side panel** | Eight tabs on the right rail: Visualizer, Scenes, Mapping, Analysis, Visual DNA, Capture, Camera, Render. All panels scroll; every control has a tooltip. |
| **Bottom bar** | MIDI LED + event count · sounding notes · polyphony / peak · current chord · last MIDI channel · capture status · quality preset and adaptive quality |

The window is freely resizable (900 × 560 up to 3840 × 2400); size, open tab, panel visibility and focus mode are
saved with the project.

### Keyboard
`F` focus mode (canvas only) · `Esc` leave focus mode · `R` reset camera · `N` next scene · `P` toggle side panel ·
`1`–`7` select a mode · `Ctrl+Z` undo · `Ctrl+Y` / `Ctrl+Shift+Z` redo. In the pop-out window `F` / `F11` toggles full screen.

## Visualization modes (all can run together as layers)

| Mode | Idea |
|---|---|
| **Piano Universe** | Every note is a 3D object. Pitch → height (or radial tunnel / spiral), time → horizontal / depth axis, velocity → size / brightness / energy, duration → length / lifespan / size, channel → depth + colour (or geometry), pitch bend → bends the trajectory. Eleven geometries (sphere, cube, ring, crystal, wireframe, particle, ribbon, cylinder, polygon, torus, abstract), six materials, spawn animation, Note-Off decay with independent *visual release*, dissolve to particles, harmonic attraction, chord clustering, collisions, trails. |
| **Constellation** | Notes are nodes; links for simultaneous notes, harmonic intervals, pitch classes, sequence, voices, octaves or a custom interval. Recognized chords become geometry (polygon, circle, radial, linear, orbital, lattice, graph, organic, abstract) labelled with the chord name; earlier chords drift away as ghosts. |
| **Flow** | Melodic trajectories per voice (ribbon, line, curve, particles, fluid, geometric). Leaps flare, repeated notes ring, ascending lines warm up. |
| **Pulse** | Rhythm as energy: rings, waves, particle bursts, oscillating geometry, rays or a breathing core. Triggers: every Note On, Note Off, velocity threshold, onset density, beat, bar, polyphony, CC or chord change. Every pulse is randomized (variation). |
| **Spectrum** | MIDI statistics (not audio): pitch, pitch class (radial), octave, velocity, duration histograms, a pitch-class heatmap and polyphony / density timelines over instant, 1 / 5 / 10 / 30 s, entire capture or custom windows; on the 3D floor, as a panel or full screen. |
| **Orbit** | Twelve pitch classes on a circle (chromatic or circle of fifths) with octave rings; notes orbit according to pitch, pitch class, octave, chord, velocity, duration, voice or harmonic relationship. |
| **Cinematic** | Ten procedural scenes — Nebula, Cathedral, Aurora, Ocean, Celestial, Crystal, Void, Bloom, Fractal, Lattice — that also decide the mix of the other layers. |

*Visualization* in the top bar picks a mode (it sets the layer mix). Switching any layer on or off by hand turns the
mode into **Custom Mix**. Each layer has its own opacity and scale.

## Scenes and transitions
Scenes are generated every frame from the music (energy, density, pitch-class activity, contour, chords, beat).
*Auto transitions* move between scenes when the chosen trigger fires — MIDI density, polyphony, average velocity,
chord change, register change, onset rate, a CC, beat, bar, a new tonal centre (pitch class), a note count, or
manual only — after the *minimum hold* time. Transition types: crossfade, dissolve, morph, expansion, collapse,
transformation, particle, geometry. *Scene blending* keeps part of the previous scene visible. Click a scene tile or
press `N` for a manual transition.

## MIDI intelligence (Analysis tab + dashboard)
Active notes, polyphony (and 2-second peak), range, average velocity, note and rhythmic density, tempo (host >
MIDI clock > estimated), chord with type, inversion, root, bass and confidence, chord transition history with voice
leading, scale estimate with confidence and the three most compatible scales, melodic contour, voices and their
ambiguity, sustain, pitch bend, last CC and per-channel activity.

* **Chords** are matched against 29 templates (triads, sus, 6ths, all common 7ths, 9ths, 11ths, 13ths, altered
  dominants) plus your own custom template, using pitch classes, the bass note and temporal stability. A new chord
  must persist for the *stabilization* time; collections below the *minimum confidence* are reported as "not a
  recognized chord" instead of being forced into a label.
* **Scales** are a probabilistic estimate from a decaying pitch-class histogram (major, natural / harmonic / melodic
  minor, major / minor pentatonic, blues, dorian, chromatic, custom).
* **Voice tracking** (MIDI channel, monophonic, pitch proximity, temporal continuity, note sequence, automatic) is a
  heuristic; the readout shows how often assignments were ambiguous.

## Mapping
Each mapping: **source** (Note On / Off, velocity, pitch, duration, channel, CC 0–127, pitch bend, channel pressure,
poly aftertouch, sustain, beat, bar, polyphony, note density, chord change) with optional channel filter →
input range → dead zone → sensitivity → invert → **curve** (linear, exponential, logarithmic, S-curve, custom with
three draggable points) → **response** (direct, envelope, accumulate, toggle) → smoothing → output range on the
**target** (any modulatable visual parameter: geometry, scale, rotation, opacity, brightness, colour, emission,
particle density / speed, trail length / persistence, camera, scene advance, distortion, material, texture,
morphing, orbit, wave, visual energy …). *MIDI Learn* assigns the next moved controller. Each row shows a live meter.

**Arbitration** when several mappings drive one target, in this order:
1. **Replace** — the last enabled Replace mapping in the list sets the value (list order = priority).
2. **Add** — each Add mapping adds `output × (max − min)` (a reversed range subtracts).
3. **Max / Min** — the value is limited to at least / at most each Max / Min mapping.
The result is clamped to the parameter range; integer and choice parameters are rounded.

## Visual DNA
A preset stores the full visual identity: mode and layers, geometry, materials, palette, camera, motion,
mappings, trails, particles, lighting, render quality, scene transitions, spatial configuration, animation and the
random seed. Categories: Favorites, Recent, Minimal, Organic, Geometric, Cinematic, Celestial, Abstract,
Experimental, Custom. Save, load, rename, duplicate, delete, import and export (`.json`). User presets live in
`%APPDATA%\EIDOLON\Presets`. Twenty factory presets are built in (also exposed to the host as programs).

**Mutation** creates controlled variations with an intensity and a seed, optionally preserving geometry, color,
motion, camera, material, particle, trail, spatial or mapping groups. *Variations* generates up to eight candidates to
pick from; *Randomize* rolls a new identity. The same seed and settings always give the same result. Every
parameter edit, preset load and mutation can be undone / redone.

## Capture
Modes: **Live buffer** (always keeps the last N seconds — press *Stop / Capture* to grab them), **Fixed duration**,
**Triggered** (starts at the first note), **Phrase** (ends after a silence gap) and **Loop**. Durations 5 / 15 / 30 /
60 s or custom. A take stores notes, velocities, channels, CCs, pitch bend, sustain and aftertouch, is shown on a
timeline, can be **replayed** internally through the visualizer, **exported** as a Standard MIDI File, or
**dragged** out of the panel into a DAW. EIDOLON does not create DAW tracks by itself.

## Camera
Static, orbit, follow, follow active notes, follow musical centre, cinematic (shot changes on chords / scenes) and
automatic (follows musical activity). Field of view, distance, angles, speed, smoothness, musical response and
impact are adjustable; user orbit / pan / zoom stays until *Reset camera*.

## Rendering and performance
Quality presets Low / Medium / High / Ultra set particle limit, trail and geometry budgets; FPS target 15–120;
anti-aliasing; motion quality; **adaptive quality** lowers detail automatically when frames take longer than the
budget; a performance overlay shows FPS, frame time, quality, note and particle counts and dropped events. On Windows
the canvas renders through OpenGL (GPU); if the GPU context cannot start within a few seconds EIDOLON falls back to
the CPU renderer automatically. *GPU acceleration* can also be turned off by hand.

## MIDI input settings (Render tab)
Channel filter, MIDI thru, stuck-note timeout (releases notes held longer than N seconds), sustain-aware note
lifecycle, and a **Panic** button.
