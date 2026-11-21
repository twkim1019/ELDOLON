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
