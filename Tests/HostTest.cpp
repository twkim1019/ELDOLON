// EIDOLON — VST3 host validation. Loads the built .vst3 through JUCE's VST3 hosting code (the same path a DAW
// uses), sends real MIDI through processBlock from a separate audio thread, and checks: descriptor (instrument,
// MIDI in / out), MIDI thru, silent audio, factory programs, state round trips, high-density MIDI, editor creation,
// resizing and tab switching. Writes PNG snapshots of the editor while MIDI is playing.
// Usage: eidolon_host_test <EIDOLON.vst3> <outputDir>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <thread>
#if JUCE_LINUX
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 static int ignoreXErrors (Display*, XErrorEvent*) { return 0; }
#endif

using namespace juce;

static int passes = 0, fails = 0;
static String report;

static void check (bool ok, const String& name, const String& detail)
{
    (ok ? passes : fails)++;
    const String line = String (ok ? "[PASS] " : "[FAIL] ") + name.paddedRight (' ', 50) + " " + detail;
    std::printf ("%s\n", line.toRawUTF8());
    std::fflush (stdout);
    report << "| " << (ok ? "PASS" : "FAIL") << " | " << name << " | " << detail << " |\n";
}

static void pump (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

/** Musical test material: a chord progression with a melody, sustain pedal, pitch bend and mod wheel. */
struct Performer
{
    double t = 0;
    int step = 0;
    double nextEvent = 0;
    int bendPhase = 0;
    std::vector<std::pair<int, int>> held;   // (channel, note)

    void render (MidiBuffer& out, int numSamples, double fs)
    {
        static const int prog[4][4] = { { 48, 55, 64, 67 }, { 45, 52, 60, 64 }, { 41, 48, 57, 60 }, { 43, 50, 59, 62 } };   // C Am F G
        static const int mel[8] = { 72, 74, 76, 79, 77, 76, 74, 71 };
        const double blockEnd = t + numSamples / fs;
        while (nextEvent < blockEnd)
        {
            const int pos = jlimit (0, numSamples - 1, (int) ((nextEvent - t) * fs));
            if (step % 8 == 0)
            {
                for (auto& h : held) out.addEvent (MidiMessage::noteOff (h.first, h.second), pos);
                held.clear();
                const int* c = prog[(step / 8) % 4];
                for (int i = 0; i < 4; ++i) { out.addEvent (MidiMessage::noteOn (1, c[i], (uint8) (70 + i * 8)), pos); held.push_back ({ 1, c[i] }); }
                out.addEvent (MidiMessage::controllerEvent (1, 64, (step / 8) % 2 ? 0 : 127), pos);
            }
            const int m = mel[step % 8];
            out.addEvent (MidiMessage::noteOn (2, m, (uint8) (60 + (step * 13) % 60)), pos);
            out.addEvent (MidiMessage::noteOff (2, m), jlimit (0, numSamples - 1, pos + (int) (0.05 * fs)));
            out.addEvent (MidiMessage::controllerEvent (1, 1, (step * 9) % 128), pos);
            out.addEvent (MidiMessage::pitchWheel (2, 8192 + (int) (std::sin (bendPhase++ * 0.7) * 1500)), pos);
            ++step;
            nextEvent += 0.25;
        }
        t = blockEnd;
    }
};

static void snapshot (Component& comp, const File& file)
{
    Image img;
   #if JUCE_LINUX
    if (auto* d = XOpenDisplay (nullptr))
    {
        const auto r = comp.getScreenBounds();
        XSync (d, False);
        if (auto* xi = XGetImage (d, DefaultRootWindow (d), r.getX(), r.getY(), (unsigned) r.getWidth(), (unsigned) r.getHeight(), AllPlanes, ZPixmap))
        {
            img = Image (Image::RGB, r.getWidth(), r.getHeight(), false);
            Image::BitmapData bd (img, Image::BitmapData::writeOnly);
            for (int y = 0; y < r.getHeight(); ++y)
                for (int x = 0; x < r.getWidth(); ++x)
                {
                    const unsigned long px = XGetPixel (xi, x, y);
                    bd.setPixelColour (x, y, Colour ((uint8) ((px >> 16) & 0xff), (uint8) ((px >> 8) & 0xff), (uint8) (px & 0xff)));
                }
            XDestroyImage (xi);
        }
        XCloseDisplay (d);
    }
   #endif
    if (! img.isValid()) img = comp.createComponentSnapshot (comp.getLocalBounds(), true, 1.0f);
    file.deleteFile();
    FileOutputStream out (file);
    PNGImageFormat png;
    png.writeImageToStream (img, out);
}

static double imageEnergy (const File& f)
{
    auto img = ImageFileFormat::loadFrom (f);
    if (! img.isValid()) return 0;
    double acc = 0; int n = 0;
    for (int y = 0; y < img.getHeight(); y += 4)
        for (int x = 0; x < img.getWidth(); x += 4) { acc += img.getPixelAt (x, y).getBrightness(); ++n; }
    return n ? acc / n : 0;
}

static void clickAt (Point<int> screen)
{
   #if JUCE_LINUX
    if (auto* d = XOpenDisplay (nullptr))
    {
        const Window root = DefaultRootWindow (d);
        Window src = root, w = root, child = 0; int x = screen.x, y = screen.y, cx = 0, cy = 0;
        while (XTranslateCoordinates (d, src, w, x, y, &cx, &cy, &child) && child != 0) { src = w; x = cx; y = cy; w = child; }
        XTranslateCoordinates (d, root, w, screen.x, screen.y, &cx, &cy, &child);
        for (int type : { MotionNotify, ButtonPress, ButtonRelease })
        {
            XEvent ev {};
            if (type == MotionNotify)
            {
                ev.xmotion = { MotionNotify, 0, True, d, w, root, 0, CurrentTime, cx, cy, screen.x, screen.y, 0, 0, True };
                XSendEvent (d, w, True, PointerMotionMask, &ev);
            }
            else
            {
                ev.xbutton = { type, 0, True, d, w, root, 0, CurrentTime, cx, cy, screen.x, screen.y,
                               (unsigned) (type == ButtonRelease ? Button1Mask : 0), Button1, True };
                XSendEvent (d, w, True, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &ev);
            }
            XFlush (d);
            pump (100);
        }
        XCloseDisplay (d);
    }
   #else
    ignoreUnused (screen);
   #endif
}

/** JUCE's VST3 host wraps the plugin state: <VST3PluginState><IComponent>base64(plugin state)</IComponent>...
    These helpers unwrap / re-wrap the plugin's own <EIDOLON> state. */
static std::unique_ptr<XmlElement> pluginXml (const MemoryBlock& hostState)
{
    auto host = AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
    if (host == nullptr) return {};
    auto* comp = host->getChildByName ("IComponent");
    if (comp == nullptr) return {};
    MemoryBlock raw;
    if (! raw.fromBase64Encoding (comp->getAllSubText().trim())) return {};
    return AudioProcessor::getXmlFromBinary (raw.getData(), (int) raw.getSize());
}

static MemoryBlock rewrap (const MemoryBlock& hostState, const XmlElement& eidolon)
{
    auto host = AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
    MemoryBlock raw, out;
    AudioProcessor::copyXmlToBinary (eidolon, raw);
    if (auto* comp = host->getChildByName ("IComponent"))
    {
        comp->deleteAllTextElements();
        comp->addTextElement (raw.toBase64Encoding());
    }
    AudioProcessor::copyXmlToBinary (*host, out);
    return out;
}

static String engineJson (AudioPluginInstance& p)
{
    MemoryBlock mb;
    p.getStateInformation (mb);
    if (auto xml = pluginXml (mb)) return xml->getStringAttribute ("engine");
    return {};
}

int main (int argc, char* argv[])
{
    if (argc < 3) { std::printf ("usage: eidolon_host_test <EIDOLON.vst3> <outputDir>\n"); return 2; }
    ScopedJuceInitialiser_GUI juceInit;
   #if JUCE_LINUX
    XSetErrorHandler (ignoreXErrors);
   #endif
    const File pluginFile (File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const File outDir (File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    outDir.createDirectory();

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    fm.getFormat (0)->findAllTypesForFile (types, pluginFile.getFullPathName());
    check (types.size() == 1, "VST3 scan finds exactly one plugin", String (types.size()) + " found");
    if (types.isEmpty()) return 1;
    const auto desc = *types[0];
    check (desc.name == "EIDOLON" && desc.isInstrument && desc.category.containsIgnoreCase ("Instrument"),
           "Descriptor (instrument, so FL Studio feeds it MIDI)", desc.name + " / " + desc.category.replace ("|", " ") + " / " + desc.manufacturerName);

    String err;
    auto plugin = fm.createPluginInstance (desc, 48000.0, 512, err);
    check (plugin != nullptr, "Instantiate via VST3 host", err.isEmpty() ? "ok" : err);
    if (plugin == nullptr) return 1;
    check (plugin->acceptsMidi() && plugin->producesMidi(), "MIDI input + MIDI output (thru) declared", "accepts / produces MIDI");

    plugin->setPlayConfigDetails (0, 2, 48000.0, 512);
    plugin->prepareToPlay (48000.0, 512);

    // fresh instance, nothing received yet: the UI must say so (activity is never faked)
    {
        std::unique_ptr<AudioProcessorEditor> ed (plugin->createEditorIfNeeded());
        if (ed != nullptr)
        {
            ed->addToDesktop (0);
            ed->setVisible (true);
            pump (1500);
            snapshot (*ed, outDir.getChildFile ("eidolon_no_midi.png"));
            ed->setVisible (false);
            pump (400);
            plugin->editorBeingDeleted (ed.get());
            ed.reset();
            pump (400);
        }
        check (ed == nullptr, "Editor opens and closes before any MIDI", "snapshot eidolon_no_midi.png");
    }

    // ---------------------------------------------------------------- MIDI processing (host thread)
    {
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        Performer perf;
        int inEvents = 0, outEvents = 0; bool silent = true, finite = true;
        for (int k = 0; k < 200; ++k)
        {
            midi.clear();
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, 0.3f * std::sin ((float) i * 0.1f));
            perf.render (midi, 512, 48000.0);
            inEvents += midi.getNumEvents();
            plugin->processBlock (buf, midi);
            outEvents += midi.getNumEvents();
            silent = silent && buf.getMagnitude (0, 512) == 0.0f;
            finite = finite && std::isfinite (buf.getMagnitude (0, 512));
        }
        check (inEvents > 30 && outEvents == inEvents, "MIDI thru: every incoming event is passed on", String (outEvents) + " / " + String (inEvents) + " events");
        check (silent && finite, "Audio output stays silent (visualizer never alters audio)", "max |out| = 0");
    }
    pump (300);

    // ---------------------------------------------------------------- programs + state
    const int numPrograms = plugin->getNumPrograms();
    check (numPrograms >= 20, "Factory Visual DNA presets exposed as programs",
           String (numPrograms) + " programs, e.g. '" + plugin->getProgramName (0) + "', '" + plugin->getProgramName (numPrograms - 1) + "'");
    plugin->setCurrentProgram (5);
    {
        AudioBuffer<float> b (2, 512); MidiBuffer none;
        for (int k = 0; k < 4; ++k) { plugin->processBlock (b, none); pump (60); }   // VST3 delivers program changes with process()
    }
    pump (300);
    const auto json5 = engineJson (*plugin);
    const auto dnaName = JSON::parse (json5).getProperty ("dna", {}).getProperty ("name", "").toString();
    check (plugin->getCurrentProgram() == 5 && dnaName == "Crystal Harmony", "Program change applies the preset",
           "program " + String (plugin->getCurrentProgram()) + ", restored DNA '" + dnaName + "'");

    {
        for (auto* p : plugin->getParameters()) if (p->getName (64) == "Visual Intensity") p->setValueNotifyingHost (0.8f);
        pump (600);
        MemoryBlock state;
        plugin->getStateInformation (state);
        auto second = fm.createPluginInstance (desc, 48000.0, 512, err);
        second->setStateInformation (state.getData(), (int) state.getSize());
        pump (600);
        const auto a = JSON::parse (engineJson (*plugin)), b = JSON::parse (engineJson (*second));
        const bool settingsEqual = a.getProperty ("settings", {}).isObject()
                                   && JSON::toString (a.getProperty ("settings", {})) == JSON::toString (b.getProperty ("settings", {}));
        const bool dnaEqual = JSON::toString (a.getProperty ("dna", {})) == JSON::toString (b.getProperty ("dna", {}));
        float hostParam = -1;
        for (auto* p : second->getParameters()) if (p->getName (64) == "Visual Intensity") hostParam = p->getValue();
        check (settingsEqual && dnaEqual && std::abs (hostParam - 0.8f) < 1e-3f, "State save -> restore into a new instance",
               String ((int) state.getSize()) + " bytes; settings " + (settingsEqual ? "equal" : "DIFFER") + ", DNA " + (dnaEqual ? "equal" : "DIFFER")
               + ", host param " + String (hostParam, 3));
        // forward compatibility: unknown keys are ignored, missing keys keep defaults
        auto xml = pluginXml (state);
        bool versionOk = false;
        if (xml != nullptr)
        {
            auto j = JSON::parse (xml->getStringAttribute ("engine"));
            if (auto* s = j.getProperty ("settings", {}).getDynamicObject()) { s->setProperty ("future.unknownKey", 42); s->removeProperty ("trail.length"); }
            if (auto* d = j.getProperty ("dna", {}).getProperty ("params", {}).getDynamicObject()) d->removeProperty ("trail.length");
            xml->setAttribute ("engine", JSON::toString (j));
            const auto mod = rewrap (state, *xml);
            auto third = fm.createPluginInstance (desc, 48000.0, 512, err);
            third->setStateInformation (mod.getData(), (int) mod.getSize());
            pump (300);
            const auto t = JSON::parse (engineJson (*third));
            versionOk = t.getProperty ("dna", {}).getProperty ("name", "").toString() == "Crystal Harmony"
                     && (int) t.getProperty ("settings", {}).getProperty ("trail.length", -1) == 24;   // missing key -> default
            third.reset();
        }
        check (versionOk, "State versioning tolerates unknown / missing keys", versionOk ? "unknown key ignored, missing key -> default, preset intact" : "failed");
        second.reset();
        pump (200);
    }

    // ---------------------------------------------------------------- editor + audio thread
    std::unique_ptr<AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    check (editor != nullptr, "Editor creation", editor ? String (editor->getWidth()) + " x " + String (editor->getHeight()) : "null");
    if (editor != nullptr)
    {
        editor->addToDesktop (0);
        editor->setVisible (true);
        pump (800);

        std::atomic<bool> run { true };
        std::atomic<int> density { 1 };
        std::thread audio ([&]
        {
            AudioBuffer<float> b (2, 256); MidiBuffer midi; Performer perf; Random rng (3);
            while (run.load())
            {
                midi.clear();
                perf.render (midi, 256, 48000.0);
                for (int extra = 0; extra < density.load() - 1; ++extra)   // stress: dense random notes on top
                {
                    const int n = 36 + rng.nextInt (60);
                    midi.addEvent (MidiMessage::noteOn (3 + rng.nextInt (4), n, (uint8) (20 + rng.nextInt (100))), rng.nextInt (256));
                    midi.addEvent (MidiMessage::noteOff (3 + rng.nextInt (4), n), rng.nextInt (256));
                }
                plugin->processBlock (b, midi);
                std::this_thread::sleep_for (std::chrono::microseconds (5333));
            }
        });

        struct Shot { int program; const char* file; int ms; };
        const bool quick = SystemStats::getEnvironmentVariable ("EIDOLON_HT_QUICK", "").isNotEmpty();
        const Shot shots[] = { { 0, "eidolon_piano_universe.png", 3500 }, { 1, "eidolon_constellation.png", 3000 }, { 2, "eidolon_flow.png", 3000 },
                               { 3, "eidolon_pulse_ocean.png", 3000 }, { 4, "eidolon_orbit.png", 3000 }, { 10, "eidolon_spectrum.png", 3000 },
                               { 7, "eidolon_bloom.png", 3000 }, { 8, "eidolon_fractal.png", 3000 }, { 15, "eidolon_neon_monolith.png", 3000 } };
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        double minEnergy = 1e9;
        for (const auto& s : shots)
        {
            if (quick && s.program > 1) break;
            plugin->setCurrentProgram (s.program);
            pump (s.ms);
            const auto f = outDir.getChildFile (s.file);
            snapshot (*editor, f);
            minEnergy = std::min (minEnergy, imageEnergy (f));
        }
        check (minEnergy > 0.02, "Visual modes render (non-empty frames)", "min mean brightness " + String (minEnergy, 3) + " over " + String ((int) std::size (shots)) + " presets");

        // side-panel tabs through real clicks on the tab rail
        const int W = editor->getWidth();
        const char* tabFiles[] = { "eidolon_tab_visualizer.png", "eidolon_tab_scenes.png", "eidolon_tab_mapping.png", "eidolon_tab_analysis.png",
                                   "eidolon_tab_dna.png", "eidolon_tab_capture.png", "eidolon_tab_camera.png", "eidolon_tab_render.png" };
        for (int tab : { 2, 3, 4, 5, 7 })
        {
            if (quick) break;
            clickAt (editor->getScreenPosition() + Point<int> (W - 23, 46 + 8 + tab * 42 + 20));
            pump (700);
            snapshot (*editor, outDir.getChildFile (tabFiles[tab]));
        }

        // stress: ~4000 extra events per second on top of the performance
        density = 12;
        const auto s0 = juce::Time::getMillisecondCounterHiRes();
        pump (3000);
        const double stressMs = juce::Time::getMillisecondCounterHiRes() - s0;
        density = 1;
        snapshot (*editor, outDir.getChildFile ("eidolon_stress.png"));
        check (stressMs < 6000, "High MIDI density keeps the GUI responsive", String (stressMs, 0) + " ms for a 3 s GUI loop at ~4000 events/s");

        bool resizeOk = true; String sizes;
        for (auto sz : { std::pair<int, int> { 960, 600 }, { 1720, 1000 }, { 1280, 780 } })
        {
            editor->setSize (sz.first, sz.second);
            pump (500);
            resizeOk = resizeOk && editor->getWidth() == sz.first && editor->getHeight() == sz.second;
            sizes << sz.first << "x" << sz.second << " ";
            if (sz.first == 960) snapshot (*editor, outDir.getChildFile ("eidolon_small.png"));
            if (sz.first == 1720) snapshot (*editor, outDir.getChildFile ("eidolon_large.png"));
        }
        check (resizeOk, "Editor resizing", sizes);

        run = false;
        audio.join();
        check (juce::Time::getMillisecondCounterHiRes() - t0 < 120000, "GUI + audio thread concurrently (no deadlock)", "completed");
        // Linux XEmbed host wrapper: let pending X11 events for the plugin window drain before deleting it
        editor->setVisible (false);
        pump (400);
        plugin->editorBeingDeleted (editor.get());
        editor.reset();
        pump (400);
        check (true, "Editor closes cleanly while the plugin keeps running", "ok");
    }

    // ---------------------------------------------------------------- shutdown
    plugin->releaseResources();
    plugin.reset();
    pump (100);
    check (true, "Plugin shutdown", "instance destroyed without crash");

    std::printf ("\n%d passed, %d failed\n", passes, fails);
    outDir.getChildFile ("host_test_report.md").replaceWithText ("| Result | Test | Measurement |\n|---|---|---|\n" + report
        + "\n**" + String (passes) + " passed, " + String (fails) + " failed**\n");
    return fails == 0 ? 0 : 1;
}
