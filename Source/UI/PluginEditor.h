#pragma once
// EIDOLON — editor: top bar (logo, MIDI status, mode, preset, undo/redo, renderer status), main canvas,
// side panel with eight tabs, bottom status bar. Resizable; focus mode and a pop-out (full-screen capable) window.
#include "../Plugin/PluginProcessor.h"
#include "Panels.h"
#include "VisualizationCanvas.h"
#include <array>

class EidolonEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EidolonEditor (EidolonProcessor&);
    ~EidolonEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectTab (int tab);
    void setFocusMode (bool on);
    void togglePopOut();
    eidolon::ui::VisualizationCanvas& canvasComponent() { return canvas; }

private:
    class PopOutWindow;
    void timerCallback() override;
    void showPresetMenu();
    void stepPreset (int delta);
    juce::Rectangle<int> topBar() const, bottomBar() const, sidePanel() const, canvasArea() const;

    EidolonProcessor& proc;
    eidolon::EidolonCore& core;
    juce::SharedResourcePointer<eidolon::ui::Fonts> fonts;
    eidolon::ui::EidolonLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    eidolon::ui::VisualizationCanvas canvas;
    juce::ComboBox modeBox;
    juce::TextButton presetButton;
    eidolon::ui::IconButton prevButton { eidolon::ui::Icon::Prev, "Previous preset" }, nextButton { eidolon::ui::Icon::Next, "Next preset" },
        undoButton { eidolon::ui::Icon::Undo, "Undo (Ctrl+Z)" }, redoButton { eidolon::ui::Icon::Redo, "Redo (Ctrl+Y)" },
        focusButton { eidolon::ui::Icon::Focus, "Focus mode: canvas only (F)", true }, popButton { eidolon::ui::Icon::PopOut, "Open the visualizer in its own window (full screen capable)", true },
        panelButton { eidolon::ui::Icon::Panel, "Show / hide the side panel (P)", true }, settingsButton { eidolon::ui::Icon::Settings, "Render settings" };
    std::array<std::unique_ptr<eidolon::ui::IconButton>, 8> tabButtons;
    std::array<std::unique_ptr<eidolon::ui::PanelBase>, 8> panels;
    int currentTab = 0;
    std::unique_ptr<PopOutWindow> popOut;
    juce::Rectangle<int> midiPill, rendererArea;
    double focusHintUntil = 0;
    uint64_t lastRevision = 0;
};
