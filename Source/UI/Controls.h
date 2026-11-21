#pragma once
// EIDOLON — reusable controls: parameter-bound rows (generated from the parameter table), lambda-bound rows
// for non-parameter data (mappings, mutation settings), and icon buttons.
#include "Theme.h"
#include "../Engine/EidolonCore.h"
#include <functional>

namespace eidolon::ui
{
juce::String formatParamValue (const ParamInfo& info, float v);

/** One row: label on the left, slider / combo / switch on the right, bound to a parameter of the core. */
class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamControl (EidolonCore& core, P param);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    P parameter() const { return param; }

private:
    EidolonCore& core;
    P param;
    const ParamInfo& info;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    bool updating = false;
};

/** Generic slider row bound to getter / setter lambdas. */
class LambdaSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    LambdaSlider (const juce::String& label, double lo, double hi, double step, std::function<float()> get,
                  std::function<void (float)> set, std::function<juce::String (float)> fmt = {});
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onGestureEnd;
    juce::Slider slider;

private:
    juce::String label;
    std::function<float()> getter;
    std::function<void (float)> setter;
    std::function<juce::String (float)> formatter;
    bool updating = false;
};

class LambdaCombo : public juce::Component, public juce::SettableTooltipClient
{
public:
    LambdaCombo (const juce::String& label, const juce::StringArray& items, std::function<int()> get, std::function<void (int)> set);
    void setItems (const juce::StringArray& items);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::ComboBox combo;

private:
    juce::String label;
    std::function<int()> getter;
    std::function<void (int)> setter;
    bool updating = false;
};

class LambdaToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    LambdaToggle (const juce::String& label, std::function<bool()> get, std::function<void (bool)> set);
    void refresh();
    void resized() override;
    juce::ToggleButton toggle;

private:
    std::function<bool()> getter;
    std::function<void (bool)> setter;
};

enum class Icon { Visualizer, Scenes, Mapping, Analysis, DNA, Capture, Camera, Render, Undo, Redo, Focus, PopOut, Panel,
                  Settings, Prev, Next, Star, Plus, Trash, Dice, Play, Stop, Record, Export, Import, Drag };

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c);

class IconButton : public juce::Button
{
public:
    IconButton (Icon i, const juce::String& tooltip, bool toggleable = false);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    Icon icon;
    bool drawBackground = false;
};

/** Text button with the EIDOLON look (thin outline, medium weight). */
class FlatButton : public juce::TextButton
{
public:
    FlatButton (const juce::String& text = {}, const juce::String& tip = {}) : juce::TextButton (text) { setTooltip (tip); }
};
} // namespace eidolon::ui
