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
| PASS | High MIDI density: analysis cost per frame | 102000 events, 0.114 ms per 60 fps frame (budget 16.6 ms) |
| PASS | History memory stays bounded | 65536 events / 8192 notes retained |

**54 passed, 0 failed**
