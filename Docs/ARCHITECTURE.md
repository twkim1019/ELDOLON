# EIDOLON — Architecture

```
 host audio thread                         message thread (engine tick, 15-120 Hz)                       paint
 ─────────────────                         ────────────────────────────────────────                       ─────
 processBlock()                            EidolonCore::tick()
  ├ read playhead → atomics      ─────►     ├ drain SpscRing<MidiEvent,16384>   (bounded per tick)
  ├ MidiBuffer → MidiEvent POD              ├ capture.onEvent / replay
  ├ SpscRing.push (wait-free;               ├ NoteTracker   → NoteChange (Start / KeyUp / Release)
  │   CCs thinned when 3/4 full,            ├ MusicalAnalyzer (chord, scale, voices, rhythm, statistics)
  │   drops counted)                        ├ MappingEngine  base params + mappings → effective params
  ├ MIDI thru (or clear)                    └ VisualizationEngine.update (world, layers, scenes, camera)
  └ buffer.clear()                                                                         VisualizationCanvas::paint
                                                                                            └ engine.render (juce::Graphics;
                                                                                              OpenGL renderer when attached)
```

* The audio thread never allocates, locks, touches files, analyses or renders. It only converts MIDI to a POD
  struct, pushes it into a fixed-size single-producer/single-consumer ring and stores the transport in atomics.
* Overflow handling: when the ring is 3/4 full continuous controllers are thinned first (notes keep priority); if it
  is completely full the event is dropped and counted (shown in the Analysis and Render tabs). Stuck-note recovery
  releases notes held longer than a timeout; *Panic* releases everything.
* The engine keeps running while the editor is closed (30 Hz), so capture and analysis do not depend on the GUI.
* With an OpenGL context attached, JUCE paints the canvas on its GL thread while holding the message-manager lock,
  so engine updates and painting never overlap.

## Modules

| Folder | Contents |
|---|---|
| `Core/` | `MidiEvent`, wait-free `SpscRing`, math (vectors, noise, reproducible RNG), **the parameter table** (`Params.h`) |
| `MIDI/` | `NoteTracker` (active / sustained / released lifecycle, controllers, stuck notes), `MidiHistory` (bounded rings) |
| `Intelligence/` | `ChordDetector`, `ScaleDetector`, `VoiceTracker`, `RhythmAnalyzer`, `MusicalStatistics`, `MusicalAnalyzer` |
| `Mapping/` | `MappingEngine` (sources, curves, responses, smoothing, arbitration) |
| `Capture/` | `CaptureEngine` (5 modes, replay), `MidiFileWriter` (SMF type 0 + parser for tests) |
| `DNA/` | `VisualDNA`, `MutationEngine` + undo history, `FactoryPresets`, `DNASerializer` (JSON), `PresetLibrary` |
| `Rendering/` | `Camera` + controller, procedural `Geometry`, `RenderContext` (3D primitives on juce::Graphics), `ParticleSystem`, `Palette` |
| `Visualization/` | shared `VisualWorld` + `FrameContext`, the six layers, `VisualizationEngine` |
| `Scenes/` | `SceneManager` (triggers, transitions, cinematic layer mix), `SceneRenderers` (10 procedural scenes) |
| `Engine/` | `EidolonCore` — the glue: queue draining, state, undo, presets, persistence |
| `Plugin/` | `EidolonProcessor` (JUCE AudioProcessor, VST3) |
| `UI/` | theme + look-and-feel, controls, generated parameter panels, mapping / DNA / capture / analysis panels, canvas, dashboard, editor |

`Core`, `MIDI`, `Intelligence`, `Mapping`, `Capture` and the JUCE-free part of `DNA` build as the static library
`eidolon_core` with **no JUCE dependency**, which is what `Tests/CoreTests.cpp` exercises.

## One parameter table
`Source/Core/Params.h` declares every setting once (key, label, group, kind, range, default, choices, mutation group,
flags, tooltip). From it EIDOLON derives the side-panel rows, Visual DNA (flag `DNA`), mapping targets (flag `MOD`),
mutation groups and JSON persistence. Adding a parameter is one line; keys are never renamed so old presets and
projects keep loading.

## State and versioning
* Plugin state = XML `<EIDOLON stateVersion=1 engine="{json}">` + the host-automatable parameters.
* The JSON holds `settings` (every parameter by key), `dna` (Visual DNA incl. mappings) and `ui` (size, tab, panel).
* Restoring ignores unknown keys, keeps defaults for missing keys and range-checks every value (tested).
* `setStateInformation` from a non-message thread is deferred to the next engine tick.

## Rendering
All visuals are drawn with `juce::Graphics` primitives through `RenderContext` (perspective projection, near-plane
clipping, glow sprites, ribbons, meshes with flat shading and six materials). On Windows the canvas attaches a
`juce::OpenGLContext`, so the same drawing code is rasterized by JUCE's OpenGL renderer on the GPU; a watchdog falls
back to the software renderer if no GL frame is produced. On the CPU path the background and vignette are cached and
the soft scene layer is rendered at half resolution. Budgets (notes drawn, trails, constellation links, particles)
scale with the quality preset and the adaptive-quality factor.
