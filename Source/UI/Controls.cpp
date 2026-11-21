#include "Controls.h"

namespace eidolon::ui
{
juce::String formatParamValue (const ParamInfo& info, float v)
{
    switch (info.kind)
    {
        case PK::Bool:   return v >= 0.5f ? "On" : "Off";
        case PK::Choice: return juce::String (info.choiceName ((int) std::lround (v)));
        case PK::Int:    return juce::String ((int) std::lround (v));
        case PK::Float:  break;
    }
    const float range = info.maxV - info.minV;
    const juce::String key (info.key);
    if (key.endsWith ("yaw") || key.endsWith ("pitch") || key.endsWith ("rotation") || key.endsWith ("lightAngle") || key.endsWith ("fov"))
        return juce::String ((int) std::lround (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
    if (key.contains ("(s)") || juce::String (info.label).contains ("(s)") || key.endsWith ("release") || key.endsWith ("transitionTime")
        || key.endsWith ("history") || key.endsWith ("life") || key.endsWith ("duration") || key.endsWith ("decay") || key.endsWith ("Window"))
        return juce::String (v, v < 10 ? 2 : 1) + " s";
    if (info.minV == 0.0f && info.maxV == 1.0f) return juce::String ((int) std::lround (v * 100.0f)) + "%";
    return juce::String (v, range <= 2.5f ? 2 : (range <= 30.0f ? 1 : 0));
}

// ------------------------------------------------------------------------------------------------- ParamControl
ParamControl::ParamControl (EidolonCore& c, P p) : core (c), param (p), info (paramInfo (p))
{
    setTooltip (juce::String (info.tooltip));
    switch (info.kind)
    {
        case PK::Float:
        case PK::Int:
        {
            slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            slider->setRange (info.minV, info.maxV, info.kind == PK::Int ? 1.0 : 0.0);
            slider->setDoubleClickReturnValue (true, info.defV);
            slider->setTooltip (juce::String (info.tooltip));
            slider->onValueChange = [this]
            {
                if (updating) return;
                core.setParam (param, (float) slider->getValue());
                if (! slider->isMouseButtonDown()) core.commitUndo();
                repaint();
            };
            slider->onDragEnd = [this] { core.commitUndo(); };
            addAndMakeVisible (*slider);
            break;
        }
        case PK::Choice:
        {
            combo = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < info.numChoices(); ++i) combo->addItem (juce::String (info.choiceName (i)), i + 1);
            combo->setTooltip (juce::String (info.tooltip));
            combo->onChange = [this]
            {
                if (updating) return;
                if (param == P::Mode) core.setMode (combo->getSelectedId() - 1);
                else { core.setParam (param, (float) (combo->getSelectedId() - 1)); core.commitUndo(); }
            };
            addAndMakeVisible (*combo);
            break;
        }
        case PK::Bool:
        {
            toggle = std::make_unique<juce::ToggleButton>();
            toggle->setTooltip (juce::String (info.tooltip));
            toggle->onClick = [this]
            {
                if (updating) return;
                core.setParam (param, toggle->getToggleState() ? 1.0f : 0.0f);
                if (paramInfo (param).group == PG::Layers) core.setParam (P::Mode, 7.0f);   // manual layer edit -> custom mix
                core.commitUndo();
            };
            addAndMakeVisible (*toggle);
            break;
        }
    }
    refresh();
}

void ParamControl::refresh()
{
    const float v = core.params().get (param);
    updating = true;
    if (slider && ! slider->isMouseButtonDown() && std::abs ((float) slider->getValue() - v) > 1e-6f) slider->setValue (v, juce::dontSendNotification);
    if (combo && combo->getSelectedId() != (int) std::lround (v) + 1) combo->setSelectedId ((int) std::lround (v) + 1, juce::dontSendNotification);
    if (toggle && toggle->getToggleState() != (v >= 0.5f)) toggle->setToggleState (v >= 0.5f, juce::dontSendNotification);
    updating = false;
    repaint();
}

void ParamControl::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::text2);
    g.setFont (Fonts::get().regular (12.5f));
    g.drawText (juce::String (info.label), r.withWidth (r.getWidth() * 0.42f), juce::Justification::centredLeft, true);
    if (slider)
    {
        g.setColour (colours::text);
        g.setFont (Fonts::get().mono (11.0f));
        g.drawText (formatParamValue (info, core.params().get (param)), r.removeFromRight (54.0f), juce::Justification::centredRight, false);
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    auto right = r.withTrimmedLeft ((int) (r.getWidth() * 0.42f));
    if (slider) slider->setBounds (right.withTrimmedRight (58).reduced (0, 4));
    if (combo) combo->setBounds (right.reduced (0, 3));
    if (toggle) toggle->setBounds (right.removeFromRight (34).reduced (0, 2));
}

// ------------------------------------------------------------------------------------------------- lambda rows
LambdaSlider::LambdaSlider (const juce::String& l, double lo, double hi, double step, std::function<float()> get,
                            std::function<void (float)> set, std::function<juce::String (float)> fmt)
    : slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox), label (l), getter (std::move (get)), setter (std::move (set)), formatter (std::move (fmt))
{
    slider.setRange (lo, hi, step);
    slider.onValueChange = [this] { if (! updating && setter) { setter ((float) slider.getValue()); repaint(); } };
    slider.onDragEnd = [this] { if (onGestureEnd) onGestureEnd(); };
    addAndMakeVisible (slider);
    refresh();
}

void LambdaSlider::refresh()
{
    if (! getter || slider.isMouseButtonDown()) return;
    updating = true;
    slider.setValue (getter(), juce::dontSendNotification);
    updating = false;
    repaint();
}

void LambdaSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::text2);
    g.setFont (Fonts::get().regular (12.5f));
    g.drawText (label, r.withWidth (r.getWidth() * 0.42f), juce::Justification::centredLeft, true);
    const float v = (float) slider.getValue();
    g.setColour (colours::text);
    g.setFont (Fonts::get().mono (11.0f));
    g.drawText (formatter ? formatter (v) : juce::String (v, slider.getInterval() >= 1.0 ? 0 : 2), r.removeFromRight (54.0f), juce::Justification::centredRight, false);
}

void LambdaSlider::resized()
{
    auto r = getLocalBounds();
    slider.setBounds (r.withTrimmedLeft ((int) (r.getWidth() * 0.42f)).withTrimmedRight (58).reduced (0, 4));
}

LambdaCombo::LambdaCombo (const juce::String& l, const juce::StringArray& items, std::function<int()> get, std::function<void (int)> set)
    : label (l), getter (std::move (get)), setter (std::move (set))
{
    setItems (items);
    combo.onChange = [this] { if (! updating && setter) setter (combo.getSelectedId() - 1); };
    addAndMakeVisible (combo);
    refresh();
}

void LambdaCombo::setItems (const juce::StringArray& items)
{
    updating = true;
    combo.clear (juce::dontSendNotification);
    for (int i = 0; i < items.size(); ++i) combo.addItem (items[i], i + 1);
    updating = false;
}

void LambdaCombo::refresh()
{
    if (! getter) return;
    updating = true;
    combo.setSelectedId (getter() + 1, juce::dontSendNotification);
    updating = false;
}

void LambdaCombo::paint (juce::Graphics& g)
{
    g.setColour (colours::text2);
    g.setFont (Fonts::get().regular (12.5f));
    g.drawText (label, getLocalBounds().toFloat().withWidth ((float) getWidth() * 0.42f), juce::Justification::centredLeft, true);
}

void LambdaCombo::resized()
{
    auto r = getLocalBounds();
    combo.setBounds (r.withTrimmedLeft ((int) (r.getWidth() * 0.42f)).reduced (0, 3));
}

LambdaToggle::LambdaToggle (const juce::String& l, std::function<bool()> get, std::function<void (bool)> set)
    : getter (std::move (get)), setter (std::move (set))
{
    toggle.setButtonText (l);
    toggle.onClick = [this] { if (setter) setter (toggle.getToggleState()); };
    addAndMakeVisible (toggle);
    refresh();
}

void LambdaToggle::refresh()
{
    if (getter) toggle.setToggleState (getter(), juce::dontSendNotification);
}

void LambdaToggle::resized() { toggle.setBounds (getLocalBounds()); }

// ------------------------------------------------------------------------------------------------- icons
void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c)
{
    const float s = std::min (r.getWidth(), r.getHeight());
    const auto b = r.withSizeKeepingCentre (s, s).reduced (s * 0.18f);
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight(), cx = b.getCentreX(), cy = b.getCentreY();
    juce::Path p;
    const juce::PathStrokeType stroke (std::max (1.2f, s * 0.07f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    bool fill = false;
    switch (icon)
    {
        case Icon::Visualizer: p.addEllipse (x, y + h * 0.25f, w * 0.5f, h * 0.5f); p.addEllipse (x + w * 0.5f, y, w * 0.5f, h * 0.5f); p.addEllipse (x + w * 0.45f, y + h * 0.6f, w * 0.38f, h * 0.38f); break;
        case Icon::Scenes:     p.addRoundedRectangle (x, y + h * 0.15f, w, h * 0.7f, 2.0f); p.startNewSubPath (x, y + h * 0.7f); p.lineTo (x + w * 0.35f, y + h * 0.42f); p.lineTo (x + w * 0.6f, y + h * 0.62f); p.lineTo (x + w, y + h * 0.35f); break;
        case Icon::Mapping:    p.addEllipse (x, y, w * 0.3f, h * 0.3f); p.addEllipse (x + w * 0.7f, y + h * 0.7f, w * 0.3f, h * 0.3f); p.startNewSubPath (x + w * 0.25f, y + h * 0.25f); p.cubicTo (x + w, y, x, y + h, x + w * 0.75f, y + h * 0.75f); break;
        case Icon::Analysis:   for (int i = 0; i < 4; ++i) { const float hh = h * (0.3f + 0.17f * (float) ((i * 7) % 4)); p.startNewSubPath (x + w * (0.12f + 0.25f * (float) i), y + h); p.lineTo (x + w * (0.12f + 0.25f * (float) i), y + h - hh); } break;
        case Icon::DNA:        for (int i = 0; i <= 12; ++i) { const float t = (float) i / 12.0f; const float yy = y + h * t; const float dx = std::sin (t * kTau) * w * 0.4f; if (i == 0) p.startNewSubPath (cx + dx, yy); else p.lineTo (cx + dx, yy); }
                               for (int i = 0; i <= 12; ++i) { const float t = (float) i / 12.0f; const float yy = y + h * t; const float dx = -std::sin (t * kTau) * w * 0.4f; if (i == 0) p.startNewSubPath (cx + dx, yy); else p.lineTo (cx + dx, yy); } break;
        case Icon::Capture:    p.addEllipse (x, y, w, h); g.setColour (c); g.fillEllipse (b.reduced (w * 0.28f)); break;
        case Icon::Camera:     p.addRoundedRectangle (x, y + h * 0.25f, w * 0.72f, h * 0.5f, 2.0f); p.startNewSubPath (x + w * 0.72f, cy); p.lineTo (x + w, y + h * 0.28f); p.lineTo (x + w, y + h * 0.72f); p.closeSubPath(); break;
        case Icon::Render:     p.addTriangle (cx, y, x + w, y + h, x, y + h); p.addEllipse (cx - w * 0.12f, y + h * 0.55f, w * 0.24f, h * 0.24f); break;
        case Icon::Undo:       p.addCentredArc (cx, cy + h * 0.08f, w * 0.38f, h * 0.38f, 0.0f, -kPi * 0.9f, kPi * 0.55f, true); p.startNewSubPath (x + w * 0.06f, y + h * 0.18f); p.lineTo (x + w * 0.14f, y + h * 0.52f); p.lineTo (x + w * 0.46f, y + h * 0.42f); break;
        case Icon::Redo:       p.addCentredArc (cx, cy + h * 0.08f, w * 0.38f, h * 0.38f, 0.0f, kPi * 0.9f, -kPi * 0.55f, true); p.startNewSubPath (x + w * 0.94f, y + h * 0.18f); p.lineTo (x + w * 0.86f, y + h * 0.52f); p.lineTo (x + w * 0.54f, y + h * 0.42f); break;
        case Icon::Focus:      for (int k = 0; k < 4; ++k) { const float sx = k % 2 ? x + w : x, sy = k / 2 ? y + h : y; const float dx = k % 2 ? -w * 0.35f : w * 0.35f, dy = k / 2 ? -h * 0.35f : h * 0.35f; p.startNewSubPath (sx + dx, sy); p.lineTo (sx, sy); p.lineTo (sx, sy + dy); } break;
        case Icon::PopOut:     p.addRoundedRectangle (x, y + h * 0.25f, w * 0.75f, h * 0.75f, 2.0f); p.startNewSubPath (x + w * 0.45f, y + h * 0.55f); p.lineTo (x + w, y); p.startNewSubPath (x + w * 0.62f, y); p.lineTo (x + w, y); p.lineTo (x + w, y + h * 0.38f); break;
        case Icon::Panel:      p.addRoundedRectangle (x, y, w, h, 2.0f); p.startNewSubPath (x + w * 0.62f, y); p.lineTo (x + w * 0.62f, y + h); break;
        case Icon::Settings:   p.addEllipse (cx - w * 0.18f, cy - h * 0.18f, w * 0.36f, h * 0.36f); for (int i = 0; i < 8; ++i) { const float a = (float) i / 8.0f * kTau; p.startNewSubPath (cx + std::cos (a) * w * 0.3f, cy + std::sin (a) * h * 0.3f); p.lineTo (cx + std::cos (a) * w * 0.5f, cy + std::sin (a) * h * 0.5f); } break;
        case Icon::Prev:       p.startNewSubPath (x + w * 0.65f, y + h * 0.15f); p.lineTo (x + w * 0.3f, cy); p.lineTo (x + w * 0.65f, y + h * 0.85f); break;
        case Icon::Next:       p.startNewSubPath (x + w * 0.35f, y + h * 0.15f); p.lineTo (x + w * 0.7f, cy); p.lineTo (x + w * 0.35f, y + h * 0.85f); break;
        case Icon::Star:       p.addStar ({ cx, cy }, 5, w * 0.22f, w * 0.5f, -kPi * 0.5f + kPi * 0.5f); break;
        case Icon::Plus:       p.startNewSubPath (cx, y); p.lineTo (cx, y + h); p.startNewSubPath (x, cy); p.lineTo (x + w, cy); break;
        case Icon::Trash:      p.addRectangle (x + w * 0.2f, y + h * 0.25f, w * 0.6f, h * 0.75f); p.startNewSubPath (x, y + h * 0.15f); p.lineTo (x + w, y + h * 0.15f); break;
        case Icon::Dice:       p.addRoundedRectangle (x, y, w, h, 3.0f); g.setColour (c); for (auto d : { 0.28f, 0.5f, 0.72f }) g.fillEllipse (x + w * d - 1.5f, y + h * d - 1.5f, 3.0f, 3.0f); break;
        case Icon::Play:       p.addTriangle (x + w * 0.2f, y, x + w, cy, x + w * 0.2f, y + h); fill = true; break;
        case Icon::Stop:       p.addRectangle (x + w * 0.15f, y + h * 0.15f, w * 0.7f, h * 0.7f); fill = true; break;
        case Icon::Record:     p.addEllipse (x + w * 0.1f, y + h * 0.1f, w * 0.8f, h * 0.8f); fill = true; break;
        case Icon::Export:     p.startNewSubPath (cx, y + h * 0.65f); p.lineTo (cx, y); p.startNewSubPath (cx - w * 0.25f, y + h * 0.25f); p.lineTo (cx, y); p.lineTo (cx + w * 0.25f, y + h * 0.25f); p.startNewSubPath (x, y + h * 0.6f); p.lineTo (x, y + h); p.lineTo (x + w, y + h); p.lineTo (x + w, y + h * 0.6f); break;
        case Icon::Import:     p.startNewSubPath (cx, y); p.lineTo (cx, y + h * 0.65f); p.startNewSubPath (cx - w * 0.25f, y + h * 0.4f); p.lineTo (cx, y + h * 0.65f); p.lineTo (cx + w * 0.25f, y + h * 0.4f); p.startNewSubPath (x, y + h * 0.6f); p.lineTo (x, y + h); p.lineTo (x + w, y + h); p.lineTo (x + w, y + h * 0.6f); break;
        case Icon::Drag:       for (int i = 0; i < 3; ++i) for (int j = 0; j < 2; ++j) { g.setColour (c); g.fillEllipse (x + w * (0.3f + 0.4f * (float) j) - 1.6f, y + h * (0.2f + 0.3f * (float) i) - 1.6f, 3.2f, 3.2f); } break;
    }
    g.setColour (c);
    if (fill) g.fillPath (p);
    else if (icon == Icon::Star) g.strokePath (p, juce::PathStrokeType (std::max (1.0f, s * 0.06f)));
    else g.strokePath (p, stroke);
}

IconButton::IconButton (Icon i, const juce::String& tip, bool toggleable) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
    setClickingTogglesState (toggleable);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (drawBackground || on || highlighted)
    {
        g.setColour (on ? colours::accent.withAlpha (0.14f) : colours::bg3.withAlpha (highlighted ? 0.9f : 0.6f));
        g.fillRoundedRectangle (r.reduced (1.0f), 5.0f);
    }
    if (on)
    {
        g.setColour (colours::accent.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 1.0f);
    }
    auto c = ! isEnabled() ? colours::text3 : on ? colours::accent : highlighted ? colours::text : colours::text2;
    if (down) c = c.brighter (0.3f);
    drawIcon (g, icon, r, c);
}
} // namespace eidolon::ui
