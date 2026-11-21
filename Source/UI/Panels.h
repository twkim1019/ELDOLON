#pragma once
// EIDOLON — side-panel content beyond the generated parameter rows.
#include "ParamPanel.h"

namespace eidolon::ui
{
class PanelBase : public juce::Component
{
public:
    virtual void refresh() {}
};

/** Wraps a ParamPanel so every tab shares the same refresh interface. */
class GeneratedPanel : public PanelBase
{
public:
    GeneratedPanel (EidolonCore& core, std::vector<ParamPanel::Section> sections, std::vector<std::unique_ptr<juce::Component>> owned = {});
    void refresh() override;
    void resized() override { panel.setBounds (getLocalBounds()); }
    std::vector<std::unique_ptr<juce::Component>> ownedComponents;

private:
    ParamPanel panel;
};

// ------------------------------------------------------------------------------------------------- mapping
class CurvePreview : public juce::Component
{
public:
    explicit CurvePreview (EidolonCore& c) : core (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<Mapping*()> mapping;

private:
    EidolonCore& core;
    int dragPoint = -1;
};

class MappingPanel : public PanelBase, private juce::Timer
{
public:
    explicit MappingPanel (EidolonCore& core);
    void refresh() override;
    void resized() override;

private:
    struct List : public juce::Component
    {
        explicit List (MappingPanel& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        MappingPanel& owner;
    };
    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        std::vector<std::pair<juce::Rectangle<float>, juce::String>> headings;
        juce::String footer;
        juce::Rectangle<float> footerArea;
    };
    void timerCallback() override;
    Mapping* selected();
    void select (int index);
    void edited (bool gestureEnd);
    void layoutContent();

    EidolonCore& core;
    int selectedIndex = -1;
    bool learning = false;
    double learnSince = 0;
    std::vector<int> targets;
    juce::Viewport viewport;
    Content content;
    List list { *this };
    FlatButton addButton { "+ Add", "Add a mapping" }, dupButton { "Duplicate" }, removeButton { "Remove" }, learnButton { "MIDI Learn", "Move a controller to assign it as the source" };
    CurvePreview curve { core };
    std::vector<std::unique_ptr<juce::Component>> rows;
    LambdaCombo* targetCombo = nullptr;
};

// ------------------------------------------------------------------------------------------------- Visual DNA
class DNAPanel : public PanelBase
{
public:
    explicit DNAPanel (EidolonCore& core);
    void refresh() override;
    void resized() override;

private:
    struct PresetList : public juce::Component
    {
        explicit PresetList (DNAPanel& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        DNAPanel& owner;
        int rowAt (int y) const { return y / 26; }
    };
    struct Variations : public juce::Component
    {
        explicit Variations (DNAPanel& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        DNAPanel& owner;
    };
    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        std::vector<std::pair<juce::Rectangle<float>, juce::String>> headings;
        juce::String status;
        juce::Rectangle<float> statusArea;
    };
    void rebuildFilter();
    void loadEntry (int filteredRow);
    void setStatus (const juce::String& s);
    const PresetEntry* selectedEntry() const;

    EidolonCore& core;
    juce::Viewport viewport;
    Content content;
    juce::TextEditor nameEditor, searchBox;
    juce::ComboBox categoryBox, filterBox;
    FlatButton saveButton { "Save", "Save the current visual identity to the user library" }, renameButton { "Rename" },
               duplicateButton { "Duplicate" }, deleteButton { "Delete" }, importButton { "Import" }, exportButton { "Export" },
               favButton { "Favorite" };
    PresetList presetList { *this };
    std::vector<int> filtered;
    int selectedRow = -1;
    float listScroll = 0;

    std::vector<std::unique_ptr<juce::Component>> mutationRows;
    FlatButton mutateButton { "Mutate", "Apply one controlled variation" }, variationsButton { "Variations", "Generate several variations to choose from" },
               randomButton { "Randomize" }, undoButton { "Undo" }, redoButton { "Redo" }, diceButton { "New seed" };
    Variations variationGrid { *this };
    int variationCount = 4;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------------------------------------- capture
class CaptureHeader : public juce::Component, private juce::Timer
{
public:
    explicit CaptureHeader (EidolonCore& core);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    EidolonCore& core;
    FlatButton startButton { "Start / Arm" }, stopButton { "Stop / Capture" }, replayButton { "Replay" }, exportButton { "Export .mid" }, clearButton { "Clear" };
    juce::Rectangle<int> dragArea, timelineArea;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragging = false;
    juce::String message;
};

// ------------------------------------------------------------------------------------------------- analysis & others
class AnalysisReadout : public juce::Component, private juce::Timer
{
public:
    explicit AnalysisReadout (EidolonCore& c) : core (c) { startTimerHz (10); }
    void paint (juce::Graphics&) override;
    static constexpr int kHeight = 470;

private:
    void timerCallback() override { repaint(); }
    EidolonCore& core;
};

class BitMaskEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    BitMaskEditor (EidolonCore& c, P param, const juce::String& title) : core (c), p (param), label (title) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    EidolonCore& core;
    P p;
    juce::String label;
};

class SceneHeader : public juce::Component, private juce::Timer
{
public:
    explicit SceneHeader (EidolonCore& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<int> cell (int i) const;
    EidolonCore& core;
    FlatButton nextButton { "Next scene", "Trigger a transition now (manual trigger)" };
};

class InfoHeader : public juce::Component, private juce::Timer
{
public:
    InfoHeader (EidolonCore& c, int kind);   // 0 = camera, 1 = render / performance, 2 = MIDI input
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<juce::String()> gpuStatus;

private:
    void timerCallback() override { repaint(); }
    EidolonCore& core;
    int kind;
    FlatButton actionButton;
};
} // namespace eidolon::ui
