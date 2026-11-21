#include "PluginEditor.h"
#include <map>
#include "../DNA/FactoryPresets.h"
#include "../Scenes/SceneManager.h"

using namespace eidolon;
using namespace eidolon::ui;

namespace
{
constexpr int kTop = 46, kBottom = 28, kRail = 46, kPanel = 318;
const char* kTabNames[] = { "Visualizer", "Scenes", "Mapping", "Analysis", "Visual DNA", "Capture", "Camera", "Render" };
const Icon kTabIcons[] = { Icon::Visualizer, Icon::Scenes, Icon::Mapping, Icon::Analysis, Icon::DNA, Icon::Capture, Icon::Camera, Icon::Render };
} // namespace

// ------------------------------------------------------------------------------------------------- pop-out window
class EidolonEditor::PopOutWindow : public juce::DocumentWindow
{
public:
    PopOutWindow (EidolonEditor& o, EidolonCore& core)
        : juce::DocumentWindow ("EIDOLON", colours::bg0, juce::DocumentWindow::allButtons), owner (o), canvas (core, false)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        canvas.setSize (1280, 720);
        setContentNonOwned (&canvas, true);
        centreWithSize (1280, 720);
        setVisible (true);
    }
    void closeButtonPressed() override { juce::MessageManager::callAsync ([&o = owner] { if (o.popOut) o.togglePopOut(); }); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::F11Key || k.getTextCharacter() == 'f') { setFullScreen (! isFullScreen()); return true; }
        if (k == juce::KeyPress::escapeKey && isFullScreen()) { setFullScreen (false); return true; }
        return false;
    }
    EidolonEditor& owner;
    VisualizationCanvas canvas;
};

// ------------------------------------------------------------------------------------------------- editor
EidolonEditor::EidolonEditor (EidolonProcessor& p) : AudioProcessorEditor (p), proc (p), core (p.core()), canvas (p.core(), true)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (canvas);

    const auto& modeInfo = paramInfo (P::Mode);
    for (int i = 0; i < modeInfo.numChoices(); ++i) modeBox.addItem (juce::String (modeInfo.choiceName (i)), i + 1);
    modeBox.setTooltip (juce::String (modeInfo.tooltip));
    modeBox.onChange = [this] { core.setMode (modeBox.getSelectedId() - 1); };
    addAndMakeVisible (modeBox);

    presetButton.setTooltip ("Visual DNA preset - click to browse");
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    undoButton.onClick = [this] { core.undo(); };
    redoButton.onClick = [this] { core.redo(); };
    focusButton.onClick = [this] { setFocusMode (focusButton.getToggleState()); };
    popButton.onClick = [this] { togglePopOut(); };
    panelButton.onClick = [this] { core.ui.panelVisible = panelButton.getToggleState(); core.markDirty(); resized(); };
    settingsButton.onClick = [this] { core.ui.panelVisible = true; selectTab (7); resized(); };
    for (auto* b : { &prevButton, &nextButton, &undoButton, &redoButton, &focusButton, &popButton, &panelButton, &settingsButton }) addAndMakeVisible (b);

    for (int i = 0; i < 8; ++i)
    {
        tabButtons[(size_t) i] = std::make_unique<IconButton> (kTabIcons[i], kTabNames[i], false);
        tabButtons[(size_t) i]->onClick = [this, i] { core.ui.panelVisible = true; selectTab (i); resized(); };
        addAndMakeVisible (*tabButtons[(size_t) i]);
    }

    using S = ParamPanel::Section;
    // Visualizer
    panels[0] = std::make_unique<GeneratedPanel> (core, std::vector<S> {
        { "Layers", paramsInGroup (PG::Layers) }, { "Piano Universe", paramsInGroup (PG::Piano) }, { "Trails", paramsInGroup (PG::Trails) },
        { "Constellation", paramsInGroup (PG::Constellation) }, { "Flow", paramsInGroup (PG::Flow) }, { "Pulse", paramsInGroup (PG::Pulse) },
        { "Spectrum", paramsInGroup (PG::Spectrum) }, { "Orbit", paramsInGroup (PG::Orbit) }, { "Look & light", paramsInGroup (PG::Look) },
        { "Particles", paramsInGroup (PG::Particles) }, { "Motion", paramsInGroup (PG::Motion) } });
    // Scenes
    {
        std::vector<std::unique_ptr<juce::Component>> owned;
        owned.push_back (std::make_unique<SceneHeader> (core));
        auto* hdr = owned.back().get();
        panels[1] = std::make_unique<GeneratedPanel> (core, std::vector<S> { { "Scenes", {}, hdr, 150 }, { "Transitions", paramsInGroup (PG::Scene) } }, std::move (owned));
    }
    panels[2] = std::make_unique<MappingPanel> (core);
    // Analysis
    {
        std::vector<std::unique_ptr<juce::Component>> owned;
        owned.push_back (std::make_unique<AnalysisReadout> (core));
        auto* readout = owned.back().get();
        owned.push_back (std::make_unique<BitMaskEditor> (core, P::AnaCustomChord, "Custom chord template (intervals from root)"));
        auto* chordMask = owned.back().get();
        owned.push_back (std::make_unique<BitMaskEditor> (core, P::AnaCustomScale, "Custom scale (pitch classes from root)"));
        auto* scaleMask = owned.back().get();
        panels[3] = std::make_unique<GeneratedPanel> (core, std::vector<S> {
            { "Live analysis", {}, readout, AnalysisReadout::kHeight }, { "Chord & scale detection", paramsInGroup (PG::Analysis) },
            { "", {}, chordMask, 46 }, { "", {}, scaleMask, 46 }, { "Dashboard", paramsInGroup (PG::Dashboard) } }, std::move (owned));
    }
    panels[4] = std::make_unique<DNAPanel> (core);
    {
        std::vector<std::unique_ptr<juce::Component>> owned;
        owned.push_back (std::make_unique<CaptureHeader> (core));
        auto* hdr = owned.back().get();
        panels[5] = std::make_unique<GeneratedPanel> (core, std::vector<S> { { "Capture", {}, hdr, 330 }, { "Settings", paramsInGroup (PG::Capture) } }, std::move (owned));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> owned;
        owned.push_back (std::make_unique<InfoHeader> (core, 0));
        auto* hdr = owned.back().get();
        panels[6] = std::make_unique<GeneratedPanel> (core, std::vector<S> { { "Camera", {}, hdr, 92 }, { "Behaviour", paramsInGroup (PG::Camera) } }, std::move (owned));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> owned;
        auto perf = std::make_unique<InfoHeader> (core, 1);
        perf->gpuStatus = [this] { return canvas.rendererName(); };
        owned.push_back (std::move (perf));
        auto* perfHdr = owned.back().get();
        owned.push_back (std::make_unique<InfoHeader> (core, 2));
        auto* midiHdr = owned.back().get();
        panels[7] = std::make_unique<GeneratedPanel> (core, std::vector<S> {
            { "Performance", {}, perfHdr, 92 }, { "Rendering", paramsInGroup (PG::Render) },
            { "MIDI input", {}, midiHdr, 92 }, { "", paramsInGroup (PG::Midi) } }, std::move (owned));
    }
    for (auto& panel : panels) addChildComponent (*panel);

    const int initialW = core.ui.width, initialH = core.ui.height;   // setResizeLimits may resize (and store) first
    setResizable (true, true);
    setResizeLimits (900, 560, 3840, 2400);
    setWantsKeyboardFocus (true);
    currentTab = core.ui.tab;
    panelButton.setToggleState (core.ui.panelVisible, juce::dontSendNotification);
    focusButton.setToggleState (core.ui.focusMode, juce::dontSendNotification);
    selectTab (currentTab);
    setSize (juce::jmax (900, initialW), juce::jmax (560, initialH));

    core.onFrame = [this]
    {
        canvas.frame();
        if (popOut) popOut->canvas.frame();
    };
    core.setEditorAttached (true);
    startTimerHz (10);
}

EidolonEditor::~EidolonEditor()
{
    stopTimer();
    core.onFrame = nullptr;
    core.setEditorAttached (false);
    popOut.reset();
    for (auto& panel : panels) panel.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EidolonEditor::selectTab (int tab)
{
    currentTab = juce::jlimit (0, 7, tab);
    core.ui.tab = currentTab;
    for (int i = 0; i < 8; ++i)
    {
        tabButtons[(size_t) i]->setToggleState (i == currentTab, juce::dontSendNotification);
        panels[(size_t) i]->setVisible (i == currentTab && core.ui.panelVisible && ! core.ui.focusMode);
    }
    panels[(size_t) currentTab]->refresh();
    core.markDirty();
}

void EidolonEditor::setFocusMode (bool on)
{
    core.ui.focusMode = on;
    focusButton.setToggleState (on, juce::dontSendNotification);
    if (on) focusHintUntil = core.uiClock() + 3.0;
    core.markDirty();
    selectTab (currentTab);
    resized();
    repaint();
}

void EidolonEditor::togglePopOut()
{
    if (popOut) { popOut.reset(); canvas.setPaused (false); }
    else { popOut = std::make_unique<PopOutWindow> (*this, core); canvas.setPaused (true); }
    popButton.setToggleState (popOut != nullptr, juce::dontSendNotification);
}

juce::Rectangle<int> EidolonEditor::topBar() const    { return core.ui.focusMode ? juce::Rectangle<int>() : getLocalBounds().removeFromTop (kTop); }
juce::Rectangle<int> EidolonEditor::bottomBar() const { return core.ui.focusMode ? juce::Rectangle<int>() : getLocalBounds().removeFromBottom (kBottom); }
juce::Rectangle<int> EidolonEditor::sidePanel() const
{
    if (core.ui.focusMode) return {};
    auto r = getLocalBounds().withTrimmedTop (kTop).withTrimmedBottom (kBottom);
    return r.removeFromRight (kRail + (core.ui.panelVisible ? kPanel : 0));
}
juce::Rectangle<int> EidolonEditor::canvasArea() const
{
    if (core.ui.focusMode) return getLocalBounds();
    return getLocalBounds().withTrimmedTop (kTop).withTrimmedBottom (kBottom).withTrimmedRight (sidePanel().getWidth());
}

void EidolonEditor::resized()
{
    canvas.setBounds (canvasArea());
    const bool chrome = ! core.ui.focusMode;
    for (auto* c : std::initializer_list<juce::Component*> { &modeBox, &presetButton, &prevButton, &nextButton, &undoButton, &redoButton, &popButton, &panelButton, &settingsButton })
        c->setVisible (chrome);
    focusButton.setVisible (true);
    for (auto& t : tabButtons) t->setVisible (chrome);

    if (chrome)
    {
        auto top = topBar().reduced (12, 8);
        top.removeFromLeft (150);                 // logo
        midiPill = top.removeFromLeft (128).reduced (0, 2);
        top.removeFromLeft (12);
        modeBox.setBounds (top.removeFromLeft (170));
        top.removeFromLeft (12);
        prevButton.setBounds (top.removeFromLeft (28));
        presetButton.setBounds (top.removeFromLeft (std::min (240, std::max (140, getWidth() / 6))));
        nextButton.setBounds (top.removeFromLeft (28));
        top.removeFromLeft (8);
        undoButton.setBounds (top.removeFromLeft (30));
        redoButton.setBounds (top.removeFromLeft (30));
        settingsButton.setBounds (top.removeFromRight (30));
        panelButton.setBounds (top.removeFromRight (30));
        popButton.setBounds (top.removeFromRight (30));
        focusButton.setBounds (top.removeFromRight (30));
        top.removeFromRight (10);
        rendererArea = top.removeFromRight (std::min (190, std::max (0, top.getWidth())));

        auto side = sidePanel();
        auto rail = side.removeFromRight (kRail);
        rail.removeFromTop (8);
        for (auto& t : tabButtons) { t->setBounds (rail.removeFromTop (kRail - 6).reduced (5, 2)); rail.removeFromTop (2); }
        for (auto& panel : panels) panel->setBounds (side.withTrimmedTop (34).reduced (2, 0));
    }
    else
    {
        focusButton.setBounds (getWidth() - 40, 8, 30, 30);
    }
    selectTab (currentTab);
    if (! core.ui.focusMode)
    {
        core.ui.width = getWidth();
        core.ui.height = getHeight();
    }
}

void EidolonEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    if (core.ui.focusMode) return;

    // ---- top bar
    const auto top = topBar();
    g.setColour (colours::bg1);
    g.fillRect (top);
    g.setColour (colours::line);
    g.fillRect (top.getX(), top.getBottom() - 1, top.getWidth(), 1);

    // logo: a small orbital sigil + tracked wordmark
    {
        const float cx = 26.0f, cy = (float) top.getCentreY();
        g.setColour (colours::accent.withAlpha (0.9f));
        g.drawEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f, 1.2f);
        juce::Path orbit;
        orbit.addCentredArc (cx, cy, 13.0f, 5.0f, -0.5f, 0.0f, kTau, true);
        g.setColour (colours::accent2.withAlpha (0.7f));
        g.strokePath (orbit, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::white);
        g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
        g.setFont (Fonts::get().caps (15.0f, 0.32f));
        g.drawText ("EIDOLON", juce::Rectangle<float> (46.0f, (float) top.getY(), 110.0f, (float) top.getHeight()), juce::Justification::centredLeft);
    }

    // MIDI status pill (driven only by received events)
    {
        const bool active = core.midiActive(), ever = core.midiEverReceived();
        const auto c = active ? colours::ok : ever ? colours::text2 : colours::warn;
        g.setColour (colours::bg3);
        g.fillRoundedRectangle (midiPill.toFloat(), 12.0f);
        g.setColour (c.withAlpha (active ? 1.0f : 0.8f));
        g.fillEllipse ((float) midiPill.getX() + 10.0f, (float) midiPill.getCentreY() - 3.5f, 7.0f, 7.0f);
        if (active) { g.setColour (c.withAlpha (0.25f)); g.fillEllipse ((float) midiPill.getX() + 6.0f, (float) midiPill.getCentreY() - 7.5f, 15.0f, 15.0f); }
        g.setColour (colours::text);
        g.setFont (Fonts::get().medium (11.5f));
        g.drawText (active ? "MIDI ACTIVE" : ever ? "MIDI IDLE" : "NO MIDI YET", midiPill.withTrimmedLeft (24), juce::Justification::centredLeft);
    }

    // renderer status
    {
        const auto& s = core.visuals().stats();
        g.setColour (colours::text3);
        g.setFont (Fonts::get().mono (10.5f));
        g.drawText (canvas.rendererName() + "  " + juce::String ((int) std::lround (s.fps)) + " fps", rendererArea, juce::Justification::centredRight, true);
    }

    // ---- side panel
    const auto side = sidePanel();
    g.setColour (colours::bg1);
    g.fillRect (side);
    g.setColour (colours::line);
    g.fillRect (side.getX(), side.getY(), 1, side.getHeight());
    g.fillRect (side.getRight() - kRail, side.getY(), 1, side.getHeight());
    if (core.ui.panelVisible)
    {
        g.setColour (colours::text);
        g.setFont (Fonts::get().caps (11.5f, 0.2f));
        g.drawText (juce::String (kTabNames[currentTab]).toUpperCase(), juce::Rectangle<int> (side.getX() + 12, side.getY(), side.getWidth() - kRail - 20, 34),
                    juce::Justification::centredLeft);
    }

    // ---- bottom bar
    const auto bot = bottomBar();
    g.setColour (colours::bg1);
    g.fillRect (bot);
    g.setColour (colours::line);
    g.fillRect (bot.getX(), bot.getY(), bot.getWidth(), 1);
    const auto& a = core.analyzer().snapshot();
    auto r = bot.reduced (12, 0);
    auto item = [&] (const juce::String& k, const juce::String& v, int w, juce::Colour vc = colours::text)
    {
        auto cell = r.removeFromLeft (w);
        g.setColour (colours::text3);
        g.setFont (Fonts::get().caps (9.5f));
        const float kw = juce::GlyphArrangement::getStringWidth (Fonts::get().caps (9.5f), k) + 8.0f;
        g.drawText (k, cell.removeFromLeft ((int) kw), juce::Justification::centredLeft);
        g.setColour (vc);
        g.setFont (Fonts::get().mono (11.0f));
        g.drawText (v, cell, juce::Justification::centredLeft, true);
    };
    {
        auto led = r.removeFromLeft (16);
        g.setColour (core.midiActive() ? colours::ok : colours::line2);
        g.fillEllipse ((float) led.getX(), (float) led.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
    item ("MIDI", juce::String ((juce::int64) core.eventsReceived()), 110);
    item ("NOTES", juce::String (a.activeNotes), 74);
    item ("POLY", juce::String (a.polyphony) + "/" + juce::String (a.peakPolyphony), 86);
    item ("CHORD", a.chord.valid ? juce::String (a.chord.name) : juce::String ("-"), 130, a.chord.valid ? colours::accent : colours::text3);
    item ("CH", core.lastChannel() >= 0 ? juce::String (core.lastChannel() + 1) : juce::String ("-"), 56);
    item ("CAPTURE", juce::String (core.capture().statusText (core.engineNow())), std::max (120, r.getWidth() - 190));
    item ("QUALITY", juce::String (paramInfo (P::RenderQuality).choiceName (core.params().geti (P::RenderQuality))) + " " + juce::String ((int) std::lround (core.visuals().stats().quality * 100)) + "%", 180);
}

void EidolonEditor::paintOverChildren (juce::Graphics& g)
{
    if (core.ui.focusMode && core.uiClock() < focusHintUntil)
    {
        auto r = juce::Rectangle<float> ((float) getWidth() * 0.5f - 150.0f, 14.0f, 300.0f, 28.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (colours::text2);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText ("Focus mode - press F or Esc to exit", r, juce::Justification::centred);
    }
}

void EidolonEditor::timerCallback()
{
    if (modeBox.getSelectedId() != core.params().geti (P::Mode) + 1) modeBox.setSelectedId (core.params().geti (P::Mode) + 1, juce::dontSendNotification);
    presetButton.setButtonText (core.currentPresetName());
    undoButton.setEnabled (core.canUndo());
    redoButton.setEnabled (core.canRedo());
    if (core.stateRevision() != lastRevision)
    {
        lastRevision = core.stateRevision();
        panels[(size_t) currentTab]->refresh();
    }
    if (! core.ui.focusMode) repaint (topBar().getUnion (bottomBar()));
    if (core.ui.focusMode && core.uiClock() < focusHintUntil + 0.2) repaint();
}

void EidolonEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& lib = core.library();
    std::map<juce::String, juce::PopupMenu> byCategory;
    for (int i = 0; i < (int) lib.entries().size(); ++i)
    {
        const auto& e = lib.entries()[(size_t) i];
        byCategory[e.category].addItem (i + 1, e.name + (e.factory ? "" : "  (user)"), true, e.name == core.currentPresetName());
    }
    juce::PopupMenu favs;
    for (int i = 0; i < (int) lib.entries().size(); ++i) if (lib.isFavorite (lib.entries()[(size_t) i].name)) favs.addItem (i + 1, lib.entries()[(size_t) i].name);
    if (favs.getNumItems() > 0) menu.addSubMenu ("Favorites", favs);
    for (int c = 2; c < kNumPresetCategories; ++c)
        if (byCategory.count (kPresetCategories[c])) menu.addSubMenu (kPresetCategories[c], byCategory[kPresetCategories[c]]);
    for (auto& [name, sub] : byCategory)
        if (! juce::StringArray (kPresetCategories, kNumPresetCategories).contains (name)) menu.addSubMenu (name, sub);
    menu.addSeparator();
    menu.addItem (10000, "Open Visual DNA browser...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int result)
    {
        if (result == 10000) { core.ui.panelVisible = true; selectTab (4); resized(); return; }
        if (result <= 0) return;
        const auto& entries = core.library().entries();
        if (result - 1 >= (int) entries.size()) return;
        VisualDNA d; juce::String err;
        if (core.library().load (entries[(size_t) result - 1], d, err))
        {
            core.loadDNA (d);
            core.library().markRecent (entries[(size_t) result - 1].name);
        }
    });
}

void EidolonEditor::stepPreset (int delta)
{
    const auto& entries = core.library().entries();
    if (entries.empty()) return;
    int idx = core.library().indexOf (core.currentPresetName());
    idx = ((idx < 0 ? 0 : idx + delta) % (int) entries.size() + (int) entries.size()) % (int) entries.size();
    VisualDNA d; juce::String err;
    if (core.library().load (entries[(size_t) idx], d, err)) core.loadDNA (d);
}

bool EidolonEditor::keyPressed (const juce::KeyPress& k)
{
    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (k.getModifiers().isCommandDown())
    {
        if (k.getKeyCode() == 'Z' || ch == 'z') { if (k.getModifiers().isShiftDown()) core.redo(); else core.undo(); return true; }
        if (k.getKeyCode() == 'Y' || ch == 'y') { core.redo(); return true; }
        return false;
    }
    if (ch == 'f') { setFocusMode (! core.ui.focusMode); return true; }
    if (k == juce::KeyPress::escapeKey && core.ui.focusMode) { setFocusMode (false); return true; }
    if (ch == 'r') { core.visuals().cameraController().reset(); return true; }
    if (ch == 'n') { core.visuals().scenes().next(); return true; }
    if (ch == 'p') { core.ui.panelVisible = ! core.ui.panelVisible; panelButton.setToggleState (core.ui.panelVisible, juce::dontSendNotification); resized(); return true; }
    if (ch >= '1' && ch <= '7') { core.setMode (ch - '1'); return true; }
    return false;
}
