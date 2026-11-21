# EIDOLON in FL Studio — MIDI routing

EIDOLON is a **visualizer that must receive MIDI**. FL Studio only sends Piano Roll and controller notes to
*generators* (Channel Rack instruments), not to effects on Mixer inserts. That is why EIDOLON is built as a
VST3 **instrument**: it accepts MIDI, passes every MIDI event through to its MIDI output (MIDI thru) and
outputs silence. It never alters audio.

> Honesty note: the routing below follows FL Studio's documented plugin behaviour. The plugin itself was built
> and tested through JUCE's VST3 hosting code (the same VST3 interfaces a DAW uses), but **it has not been run
> inside FL Studio by this build pipeline** (no FL Studio in the build environment). If something differs on your
> system, check the MIDI status pill in EIDOLON's top bar — it only turns green when MIDI actually arrives.

## 1. Install and scan

1. Run `EIDOLON-1.0.0-Setup.exe` (or copy `EIDOLON.vst3` to `C:\Program Files\Common Files\VST3\`).
2. FL Studio: **Options → Manage plugins → Find installed plugins** (or *Start scan*).
3. EIDOLON appears in the **Generators** list (it is an instrument). Tick the star to make it a favorite.

## 2. Quickest setup — EIDOLON on its own channel

1. **Channel Rack → + (Add) → EIDOLON**.
2. Open the EIDOLON channel's **Piano Roll** and draw notes, or select the EIDOLON channel and play your MIDI
   keyboard (FL sends controller input to the selected channel).
3. The top-bar pill changes from **NO MIDI YET** to **MIDI ACTIVE**.

EIDOLON produces no sound, so this is ideal for watching a part you already hear elsewhere.

## 3. See and hear the same notes

### Option A — Layer channel (simplest)
1. **Channel Rack → + → Layer**.
2. Select your instrument channel and the EIDOLON channel (Ctrl+click), open the Layer channel settings and press
   **Set children**.
3. Write notes in the **Layer** channel's Piano Roll (or select the Layer and play your keyboard). Both the instrument
   and EIDOLON receive the same notes.

### Option B — MIDI thru with ports
EIDOLON forwards all incoming MIDI to its MIDI output (Render panel → MIDI input → *MIDI thru*, on by default).
1. Open EIDOLON's wrapper settings (the gear icon in the plugin window title bar) → MIDI → set **Output port** to a
   free number, e.g. **10**.
2. Open your instrument's wrapper settings → MIDI → set **Input port** to **10**.
3. Play/record into the EIDOLON channel; the instrument receives the notes through the port.

## 4. External MIDI controller

1. **Options → MIDI settings**: enable your controller in the Input list.
2. Select the EIDOLON (or Layer) channel in the Channel Rack and play.
3. Sustain pedal, mod wheel and other CCs, pitch bend and aftertouch reach the plugin: FL passes VST3 controller
   data through the plugin's MIDI mapping, and EIDOLON (JUCE) converts it back to MIDI CC / pitch bend.

## 5. Things that do NOT give EIDOLON MIDI

* Putting EIDOLON on a **Mixer insert** (it will not appear there anyway, because it is an instrument).
* Playing a *different* channel than the one EIDOLON (or its Layer) is on.

## 6. Tips

* **Tempo / beat / bar** come from FL's transport while it plays; when stopped, EIDOLON estimates tempo from the notes.
* **Keyboard focus**: while EIDOLON's window is focused, the keys F, R, N, P and 1–7 go to EIDOLON. Click FL's window
  to give focus back.
* **Pop-out window**: the top-bar pop-out button opens the visualizer in its own window (F11 / F = full screen) —
  useful on a second monitor.
* **Capture → MIDI**: EIDOLON cannot create an FL Studio track by itself. Export the take as a `.mid` file from the
  Capture panel, or drag the take from the Capture panel onto FL Studio (drag-and-drop of the temporary `.mid` file;
  untested inside FL Studio by this pipeline).
* **Host automation**: *Visual Intensity*, *Motion Speed*, *Release Scale*, *Scene* and *Next Scene* are automatable
  (Browse parameters / link to controller).
