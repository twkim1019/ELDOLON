# EIDOLON Test Report

Date: 2026-11-21. Every result below was produced by running the test programs in `Tests/`; nothing listed as
passed was assumed.

## Environment
* Linux x86_64 (2 cores), GCC 13.3, CMake 3.28, Ninja, JUCE 8.0.4
* The plugin was built as a **Linux VST3 from the same sources** as the Windows target
* GUI tests under Xvfb (virtual X display, **software renderer**, no GPU, no window manager)
* Windows results come from GitHub Actions (section 4)
* Not covered: FL Studio itself (not available in either environment) and an interactive check of the OpenGL path
  on real GPU hardware

## 1. Core test suite — `Tests/CoreTests.cpp`
MIDI engine, musical intelligence, mapping, capture / Standard MIDI File export, Visual DNA, performance.

| Result | Test | Measurement |
|---|---|---|
| PASS | Note On / Note Off lifecycle | 2 changes, active after off = 0 |
| PASS | Velocity and timestamps preserved | vel 90, on 0.00, off 0.50 |
| PASS | Note On velocity 0 = Note Off | 2 changes |
| PASS | Sustain pedal holds released keys | held=1, released at 0.5 s |
| PASS | Sustain-aware can be disabled | active 0 |
| PASS | Pitch bend tracking (-8192..8191 -> -1..1) | -1.000 / 0.000 / 1.000 |
| PASS | CC tracking + All Notes Off (CC 123) | CC1 = 0.504, active after CC123 = 0 |
| PASS | Poly aftertouch + channel pressure | poly AT 77, pressure 1.00 |
| PASS | Multiple channels are independent | active 1 (ch2 still on) |
| PASS | Simultaneous notes (polyphony, range, pitch classes) | active 4, low 60, high 71 |
| PASS | Repeated note re-trigger (release + new instance) | 3 changes, ids 1 -> 2 |
| PASS | Stuck-note recovery (timeout) | released 1 stuck note, 1 still active |
| PASS | Panic releases everything | active 0 |
| PASS | Event queue overflow is bounded and detected | 1024 of 1500 accepted (capacity 1024), 476 dropped |
| PASS | Event ordering (FIFO through the lock-free queue) | 1024 events in order |
| PASS | Chord recognition (20 chord types, root position) | 20 / 20 correct |
| PASS | Inversions (1st / 2nd / 3rd) + bass note | C/E, C/G, C7/A# |
| PASS | Bass context disambiguates (Am7 vs C6) | Am7 / C6 |
| PASS | Root estimation in inversion (A C E over C) | Am/C |
| PASS | Non-chords are not forced into chord labels | cluster none, single none, dyad (min 3) none |
| PASS | Dyads with min-notes 2 (power chord / interval) | C5, C-E Interval M3 |
| PASS | Chord confidence drops with foreign tones | clean 1.00 vs +F# 0.46 (C) |
| PASS | Chord smoothing suppresses flicker | 2 committed changes (C -> G, 40 ms F skipped) |
| PASS | Chord transition history + voice leading | root motion -5, nearest-voice movement 8 semitones |
| PASS | Scale estimation (major / harmonic minor / pentatonic) | C Major, A Harmonic Minor, C Major Pentatonic |
| PASS | Scale confidence + alternative candidates | C major conf 0.60; blues-ish line -> A Blues (conf 0.85) |
| PASS | Pitch / pitch-class / octave distribution | 12 notes, PC C = 12, oct5 4, range 60-84 |
| PASS | Instant window = sounding notes only | 0 sounding |
| PASS | Voice tracking separates interleaved lines | 32 / 32 assignments consistent, ambiguity 0.01 |
| PASS | Voice tracking by MIDI channel | ch3 -> voice 2, ch2 -> voice 1 |
| PASS | Tempo estimation from onsets (120 BPM) | 120.0 BPM (estimated) |
| PASS | Onset rate / rhythmic density | 8.00 onsets/s, density 0.67 |
| PASS | Beat / bar phase from host transport | beat phase 0.500, bar phase 0.625, 96 BPM |
| PASS | Analyzer pipeline: polyphony + chord + range | poly 5, chord C, range 48-72 |
| PASS | Mapping curves monotonic with fixed endpoints | linear / exp / log / S / custom |
| PASS | Arbitration: Replace -> Add -> Min | 0.5 + 0.2 limited to 0.6 = 0.600 |
| PASS | Multiple mappings on one target (Replace + Add) | 0.700 |
| PASS | Envelope response (Note On impulse decays) | peak 1.00 -> 0.091 after 0.48 s |
| PASS | Toggle response flips on each press | 1 -> 0 |
| PASS | Dead zone + inversion | input 0.1 in 0.2 dead zone, inverted -> 1.00 |
| PASS | Fixed-duration capture stops itself | 25 notes in 5.00 s take |
| PASS | Triggered capture starts on first note | take starts at first note, 2 events |
| PASS | Phrase capture ends after silence gap | 8 notes, 2.55 s |
| PASS | Live buffer keeps only the last N seconds | 50 notes in last 5.0 s |
| PASS | Internal replay emits every captured event | 100 / 100 events replayed |
| PASS | Standard MIDI File export (parse-back) | 96 bytes: 3 on / 3 off (2 hanging closed), 1 CC, 1 bend, end tick 3840 |
| PASS | Parameter table (unique keys, valid defaults, choice counts) | 196 parameters |
| PASS | Factory Visual DNA presets valid | 20 presets |
| PASS | Mutation is reproducible (same seed = same DNA) | 89 parameters changed; seed 4243 differs |
| PASS | Mutation preserves selected groups | Color + Camera untouched |
| PASS | Variation count + distinct variations | 6 variations, 6 distinct sizes |
| PASS | Undo / redo history | push 3, undo 2, redo 1 |
| PASS | High MIDI density: analysis cost per frame | 102000 events, 0.121 ms per 60 fps frame (budget 16.6 ms) |
| PASS | History memory stays bounded | 65536 events / 8192 notes retained |

**54 passed, 0 failed**

## 2. VST3 host validation (Linux) — `Tests/HostTest.cpp`
The built `EIDOLON.vst3` is scanned and loaded through JUCE's `VST3PluginFormat` (the hosting path a DAW uses).
MIDI is sent through `processBlock` (also from a separate audio thread while the editor runs); state is read back
from the host-side VST3 state container.

| Result | Test | Measurement |
|---|---|---|
| PASS | VST3 scan finds exactly one plugin | 1 found |
| PASS | Descriptor (instrument, so FL Studio feeds it MIDI) | EIDOLON / Instrument Tools / EIDOLON Lab |
| PASS | Instantiate via VST3 host | ok |
| PASS | MIDI input + MIDI output (thru) declared | accepts / produces MIDI |
| PASS | Editor opens and closes before any MIDI | snapshot eidolon_no_midi.png |
| PASS | MIDI thru: every incoming event is passed on | 50 / 50 events |
| PASS | Audio output stays silent (visualizer never alters audio) | max |out| = 0 |
| PASS | Factory Visual DNA presets exposed as programs | 20 programs, e.g. 'First Light', 'Solar Choir' |
| PASS | Program change applies the preset | program 5, restored DNA 'Crystal Harmony' |
| PASS | State save -> restore into a new instance | 21113 bytes; settings equal, DNA equal, host param 0.800 |
| PASS | State versioning tolerates unknown / missing keys | unknown key ignored, missing key -> default, preset intact |
| PASS | Editor creation | 1280 x 780 |
| PASS | Visual modes render (non-empty frames) | min mean brightness 0.095 over 9 presets |
| PASS | High MIDI density keeps the GUI responsive | 3027.6 ms for a 3 s GUI loop at ~4000 events/s |
| PASS | Editor resizing | 960x600 1720x1000 1280x780  |
| PASS | GUI + audio thread concurrently (no deadlock) | completed |
| PASS | Editor closes cleanly while the plugin keeps running | ok |
| PASS | Plugin shutdown | instance destroyed without crash |

**18 passed, 0 failed**

Notes:
* *High MIDI density*: ~4000 extra random note events per second on 4 channels on top of a musical performance,
  while the editor renders on the CPU renderer. The 3-second GUI loop completed in about 3 s (no stall).
* Software rendering under Xvfb is slow (adaptive quality drops to its floor); the Windows build renders through
  OpenGL.

## 3. Windows x64 — GitHub Actions run 44872987068 (windows-2022, MSVC 2022)
| Step | Result |
|---|---|
| Configure + Release build (`EIDOLON_ENABLE_OPENGL=ON`, static runtime) | success (12 compiler warnings, 0 errors) |
| Core tests (`eidolon_core_tests.exe`) | 54 / 54 passed |
| pluginval, strictness 5, `--skip-gui-tests` (plugin info, programs, audio processing, state, automation, automatable parameters, buses, VST3 validator) | SUCCESS, exit code 0 |
| Inno Setup 6 → `EIDOLON-1.0.0-Setup.exe` (4.7 MB, contains `Contents\x86_64-win\EIDOLON.vst3`) | success, pushed to the `binaries` branch |

## 4. Screenshots (`Docs/screenshots/`)
Taken from the running plugin editor under Xvfb (CPU renderer) while MIDI was being played:

* `eidolon_no_midi.png`
* `eidolon_piano_universe.png`
* `eidolon_constellation.png`
* `eidolon_flow.png`
* `eidolon_pulse_ocean.png`
* `eidolon_orbit.png`
* `eidolon_spectrum.png`
* `eidolon_bloom.png`
* `eidolon_fractal.png`
* `eidolon_neon_monolith.png`
* `eidolon_tab_mapping.png`
* `eidolon_tab_analysis.png`
* `eidolon_tab_dna.png`
* `eidolon_tab_capture.png`
* `eidolon_tab_render.png`
* `eidolon_stress.png`
* `eidolon_small.png`
* `eidolon_large.png`
