// EIDOLON — mapping editor: list of MIDI->visual mappings with live output meters, MIDI learn, and an editor
// for source / target / ranges / curve (with draggable custom curve) / smoothing / sensitivity / inversion /
// dead zone / response / arbitration.
#include "Panels.h"

namespace eidolon::ui
{
GeneratedPanel::GeneratedPanel (EidolonCore& core, std::vector<ParamPanel::Section> sections, std::vector<std::unique_ptr<juce::Component>> owned)
    : ownedComponents (std::move (owned)), panel (core, std::move (sections))
{
    addAndMakeVisible (panel);
}

void GeneratedPanel::refresh() { panel.refresh(); }

// ------------------------------------------------------------------------------------------------- curve preview
void CurvePreview::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::line);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 4.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    }
    Mapping* m = mapping ? mapping() : nullptr;
    if (m == nullptr)
    {
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText ("No mapping selected", r, juce::Justification::centred);
        return;
    }
    juce::Path p;
    for (int i = 0; i <= 64; ++i)
    {
        const float x = (float) i / 64.0f;
        const float y = applyCurve (*m, x);
        const float px = r.getX() + x * r.getWidth(), py = r.getBottom() - y * r.getHeight();
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (colours::accent);
    g.strokePath (p, juce::PathStrokeType (1.6f));
    // live input marker
    const float lx = r.getX() + saturate (m->lastInput) * r.getWidth();
    const float ly = r.getBottom() - applyCurve (*m, saturate (m->lastInput)) * r.getHeight();
    g.setColour (colours::warm);
    g.fillEllipse (lx - 3.5f, ly - 3.5f, 7.0f, 7.0f);
    if (m->curve == MapCurve::Custom)
        for (int k = 0; k < 3; ++k)
        {
            const float px = r.getX() + 0.25f * (float) (k + 1) * r.getWidth(), py = r.getBottom() - m->customPoints[(size_t) k] * r.getHeight();
            g.setColour (juce::Colours::white);
            g.drawEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.3f);
        }
    g.setColour (colours::text3);
    g.setFont (Fonts::get().caps (9.5f));
    g.drawText (m->curve == MapCurve::Custom ? "DRAG POINTS" : "CURVE", r.reduced (6.0f, 4.0f), juce::Justification::topLeft);
}

void CurvePreview::mouseDown (const juce::MouseEvent& e)
{
    Mapping* m = mapping ? mapping() : nullptr;
    dragPoint = -1;
    if (m == nullptr || m->curve != MapCurve::Custom) return;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const int k = juce::jlimit (0, 2, (int) std::lround ((e.position.x - r.getX()) / r.getWidth() * 4.0f) - 1);
    dragPoint = k;
    mouseDrag (e);
}

void CurvePreview::mouseDrag (const juce::MouseEvent& e)
{
    Mapping* m = mapping ? mapping() : nullptr;
    if (m == nullptr || dragPoint < 0) return;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    m->customPoints[(size_t) dragPoint] = saturate ((r.getBottom() - e.position.y) / r.getHeight());
    core.markDirty();
    repaint();
}

void CurvePreview::mouseUp (const juce::MouseEvent&)
{
    if (dragPoint >= 0) core.commitUndo();
    dragPoint = -1;
}

// ------------------------------------------------------------------------------------------------- mapping panel
MappingPanel::MappingPanel (EidolonCore& c) : core (c)
{
    for (int i = 0; i < kNumParams; ++i) if (paramInfo (i).isModulatable()) targets.push_back (i);
    juce::StringArray targetNames;
    for (int t : targets) targetNames.add (juce::String (groupName (paramInfo (t).group)) + "  /  " + paramInfo (t).label);
    juce::StringArray sources, curves, responses, blends, channels { "Any" };
    for (int i = 0; i < (int) MapSource::Count; ++i) sources.add (mapSourceName ((MapSource) i));
    for (int i = 0; i < (int) MapCurve::Count; ++i) curves.add (mapCurveName ((MapCurve) i));
    for (int i = 0; i < (int) MapResponse::Count; ++i) responses.add (mapResponseName ((MapResponse) i));
    for (int i = 0; i < (int) MapBlend::Count; ++i) blends.add (mapBlendName ((MapBlend) i));
    for (int i = 1; i <= 16; ++i) channels.add (juce::String (i));

    auto sel = [this] { return selected(); };
    auto pct = [] (float v) { return juce::String ((int) std::lround (v * 100.0f)) + "%"; };
    auto addRow = [this] (juce::Component* comp) { rows.emplace_back (comp); content.addAndMakeVisible (comp); return comp; };
    auto slider = [&, sel] (const juce::String& name, double lo, double hi, double step, std::function<float& (Mapping&)> field, std::function<juce::String (float)> fmt, const juce::String& tip)
    {
        auto* s = new LambdaSlider (name, lo, hi, step,
                                    [sel, field] { auto* m = sel(); return m ? field (*m) : 0.0f; },
                                    [this, sel, field] (float v) { if (auto* m = sel()) { field (*m) = v; edited (false); } }, fmt);
        s->onGestureEnd = [this] { edited (true); };
        s->setTooltip (tip);
        return addRow (s);
    };
    auto combo = [&, sel] (const juce::String& name, const juce::StringArray& items, std::function<int (Mapping&)> get, std::function<void (Mapping&, int)> set, const juce::String& tip)
    {
        auto* cb = new LambdaCombo (name, items, [sel, get] { auto* m = sel(); return m ? get (*m) : -1; },
                                    [this, sel, set] (int v) { if (auto* m = sel()) { set (*m, v); edited (true); } });
        cb->setTooltip (tip);
        return static_cast<LambdaCombo*> (addRow (cb));
    };

    combo ("Source", sources, [] (Mapping& m) { return (int) m.source; }, [] (Mapping& m, int v) { m.source = (MapSource) v; m.resetRuntime(); }, "MIDI data that drives the mapping");
    {
        auto* s = new LambdaSlider ("CC number", 0, 127, 1, [sel] { auto* m = sel(); return m ? (float) m->ccNumber : 0.0f; },
                                    [this, sel] (float v) { if (auto* m = sel()) { m->ccNumber = (int) v; edited (false); } });
        s->onGestureEnd = [this] { edited (true); };
        s->setTooltip ("Controller number used when the source is CC (1 = mod wheel, 64 = sustain, 74 = brightness)");
        addRow (s);
    }
    combo ("Channel", channels, [] (Mapping& m) { return m.channel; }, [] (Mapping& m, int v) { m.channel = v; }, "Only respond to this MIDI channel");
    targetCombo = combo ("Target", targetNames,
                         [this] (Mapping& m) { for (size_t i = 0; i < targets.size(); ++i) if (targets[i] == m.target) return (int) i; return -1; },
                         [this] (Mapping& m, int v) { if (v >= 0 && v < (int) targets.size()) m.target = targets[(size_t) v]; },
                         "Visual parameter driven by the mapping");
    slider ("Output min", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.outMin; }, pct, "Normalized target value at input 0 (Add: depth start)");
    slider ("Output max", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.outMax; }, pct, "Normalized target value at input 1");
    slider ("Input range min", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.inMin; }, pct, "Input values below this map to 0");
    slider ("Input range max", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.inMax; }, pct, "Input values above this map to 1");
    slider ("Dead zone", 0, 0.9, 0.001, [] (Mapping& m) -> float& { return m.deadZone; }, pct, "Ignore small input values");
    slider ("Sensitivity", 0.1, 4, 0.01, [] (Mapping& m) -> float& { return m.sensitivity; }, {}, "Input gain before the curve");
    {
        auto* t = new LambdaToggle ("Invert", [sel] { auto* m = sel(); return m != nullptr && m->invert; },
                                    [this, sel] (bool b) { if (auto* m = sel()) { m->invert = b; edited (true); } });
        addRow (t);
    }
    combo ("Curve", curves, [] (Mapping& m) { return (int) m.curve; }, [] (Mapping& m, int v) { m.curve = (MapCurve) v; }, "Response curve (Custom: drag the three points)");
    slider ("Curve amount", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.curveAmount; }, pct, "Strength of exponential / logarithmic / S curves");
    combo ("Response", responses, [] (Mapping& m) { return (int) m.response; }, [] (Mapping& m, int v) { m.response = (MapResponse) v; m.resetRuntime(); },
           "Direct follows the input, Envelope reacts to events and decays, Accumulate integrates (rotation), Toggle flips");
    slider ("Decay / rate", 0.02, 4, 0.001, [] (Mapping& m) -> float& { return m.decay; }, [] (float v) { return juce::String (v, 2) + " s"; }, "Envelope decay time or accumulate rate");
    slider ("Smoothing", 0, 1, 0.001, [] (Mapping& m) -> float& { return m.smoothing; }, pct, "Low-pass smoothing of the output");
    combo ("Arbitration", blends, [] (Mapping& m) { return (int) m.blend; }, [] (Mapping& m, int v) { m.blend = (MapBlend) v; },
           "How this mapping combines with others on the same target");

    curve.mapping = sel;
    content.addAndMakeVisible (curve);
    content.addAndMakeVisible (list);
    for (auto* b : { &addButton, &dupButton, &removeButton, &learnButton }) content.addAndMakeVisible (b);
    learnButton.setClickingTogglesState (true);
    content.footer = "Arbitration: Replace mappings apply in list order (the last one wins), then every Add mapping adds "
                     "output x (max - min), then Max / Min mappings limit the value. Results are clamped to the parameter range.";

    addButton.onClick = [this]
    {
        Mapping m;
        m.source = MapSource::CC; m.ccNumber = 1; m.target = (int) P::MotionEnergy; m.outMin = 0; m.outMax = 0.5f; m.blend = MapBlend::Add;
        core.dnaMeta().mappings.push_back (m);
        select ((int) core.dnaMeta().mappings.size() - 1);
        edited (true);
    };
    dupButton.onClick = [this] { if (auto* m = selected()) { Mapping c = *m; c.resetRuntime(); core.dnaMeta().mappings.push_back (c); select ((int) core.dnaMeta().mappings.size() - 1); edited (true); } };
    removeButton.onClick = [this]
    {
        auto& maps = core.dnaMeta().mappings;
        if (selectedIndex >= 0 && selectedIndex < (int) maps.size())
        {
            maps.erase (maps.begin() + selectedIndex);
            select (std::min (selectedIndex, (int) maps.size() - 1));
            edited (true);
        }
    };
    learnButton.onClick = [this]
    {
        learning = learnButton.getToggleState() && selected() != nullptr;
        learnButton.setToggleState (learning, juce::dontSendNotification);
        const auto& cc = core.analyzer().snapshot().recentCC[0];
        learnSince = cc.time;
    };

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    select (core.dnaMeta().mappings.empty() ? -1 : 0);
    startTimerHz (15);
}

Mapping* MappingPanel::selected()
{
    auto& maps = core.dnaMeta().mappings;
    return (selectedIndex >= 0 && selectedIndex < (int) maps.size()) ? &maps[(size_t) selectedIndex] : nullptr;
}

void MappingPanel::select (int index)
{
    selectedIndex = index;
    for (auto& r : rows)
    {
        if (auto* s = dynamic_cast<LambdaSlider*> (r.get())) s->refresh();
        if (auto* c = dynamic_cast<LambdaCombo*> (r.get())) c->refresh();
        if (auto* t = dynamic_cast<LambdaToggle*> (r.get())) t->refresh();
        r->setEnabled (selected() != nullptr);
    }
    list.repaint();
    curve.repaint();
}

void MappingPanel::edited (bool gestureEnd)
{
    core.markDirty();
    if (gestureEnd) core.commitUndo();
    list.repaint();
    curve.repaint();
}

void MappingPanel::refresh()
{
    if (selectedIndex >= (int) core.dnaMeta().mappings.size()) select ((int) core.dnaMeta().mappings.size() - 1);
    else select (selectedIndex);
    layoutContent();
}

void MappingPanel::timerCallback()
{
    if (learning)
    {
        const auto& cc = core.analyzer().snapshot().recentCC[0];
        if (cc.time > learnSince && cc.channel >= 0)
            if (auto* m = selected())
            {
                m->source = MapSource::CC;
                m->ccNumber = cc.number;
                m->channel = cc.channel + 1;
                learning = false;
                learnButton.setToggleState (false, juce::dontSendNotification);
                select (selectedIndex);
                edited (true);
            }
    }
    list.repaint();
    curve.repaint();
}

void MappingPanel::layoutContent()
{
    const int w = std::max (200, getWidth() - 12);
    int y = 6;
    content.headings.clear();
    content.headings.push_back ({ { 8.0f, (float) y + 6.0f, (float) w - 8.0f, 16.0f }, "Mappings" });
    y += 28;
    const int bw = (w - 8 - 18) / 4;
    addButton.setBounds (8, y, bw, 26);
    dupButton.setBounds (8 + (bw + 6), y, bw, 26);
    removeButton.setBounds (8 + 2 * (bw + 6), y, bw, 26);
    learnButton.setBounds (8 + 3 * (bw + 6), y, bw, 26);
    y += 34;
    const int listH = std::max (3, (int) core.dnaMeta().mappings.size()) * 30 + 4;
    list.setBounds (8, y, w - 8, listH);
    y += listH + 8;
    content.headings.push_back ({ { 8.0f, (float) y + 6.0f, (float) w - 8.0f, 16.0f }, "Selected mapping" });
    y += 28;
    curve.setBounds (8, y, w - 8, 110);
    y += 118;
    for (auto& r : rows) { r->setBounds (8, y, w - 8, 28); y += 28; }
    y += 8;
    content.footerArea = { 8.0f, (float) y, (float) w - 8.0f, 74.0f };
    y += 80;
    content.setBounds (0, 0, w, y);
}

void MappingPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutContent();
}

void MappingPanel::Content::paint (juce::Graphics& g)
{
    for (auto& h : headings) drawSectionHeading (g, h.first, h.second);
    g.setColour (colours::text3);
    g.setFont (Fonts::get().regular (11.5f));
    g.drawFittedText (footer, footerArea.toNearestInt(), juce::Justification::topLeft, 5);
}

void MappingPanel::List::paint (juce::Graphics& g)
{
    auto& maps = owner.core.dnaMeta().mappings;
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    if (maps.empty())
    {
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText ("No mappings. Press + Add.", getLocalBounds(), juce::Justification::centred);
        return;
    }
    for (int i = 0; i < (int) maps.size(); ++i)
    {
        const auto& m = maps[(size_t) i];
        auto r = juce::Rectangle<int> (0, 2 + i * 30, getWidth(), 28).reduced (4, 1);
        if (i == owner.selectedIndex) { g.setColour (colours::accent.withAlpha (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f); }
        // enable switch
        auto sw = r.removeFromLeft (22).withSizeKeepingCentre (14, 14).toFloat();
        g.setColour (m.enabled ? colours::accent : colours::line2);
        g.drawEllipse (sw, 1.2f);
        if (m.enabled) g.fillEllipse (sw.reduced (3.5f));
        // delete glyph
        auto del = r.removeFromRight (20);
        drawIcon (g, Icon::Trash, del.toFloat().withSizeKeepingCentre (14, 14), colours::text3);
        // live meter
        auto meter = r.removeFromRight (46).withSizeKeepingCentre (40, 4).toFloat();
        g.setColour (colours::bg3.brighter (0.1f));
        g.fillRoundedRectangle (meter, 2.0f);
        g.setColour (colours::warm);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * saturate (m.output)), 2.0f);
        juce::String src = mapSourceName (m.source);
        if (m.source == MapSource::CC) src = "CC" + juce::String (m.ccNumber);
        if (m.channel > 0) src << " ch" << m.channel;
        g.setColour (m.enabled ? colours::text : colours::text3);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText (src + juce::String (juce::CharPointer_UTF8 ("  \xe2\x86\x92  ")) + paramInfo (m.target).label + "  (" + mapBlendName (m.blend) + ")",
                    r.reduced (4, 0), juce::Justification::centredLeft, true);
    }
}

void MappingPanel::List::mouseDown (const juce::MouseEvent& e)
{
    auto& maps = owner.core.dnaMeta().mappings;
    const int i = (e.y - 2) / 30;
    if (i < 0 || i >= (int) maps.size()) return;
    if (e.x < 28) { maps[(size_t) i].enabled = ! maps[(size_t) i].enabled; owner.edited (true); }
    else if (e.x > getWidth() - 26)
    {
        maps.erase (maps.begin() + i);
        owner.select (std::min (owner.selectedIndex, (int) maps.size() - 1));
        owner.edited (true);
        owner.layoutContent();
        return;
    }
    owner.select (i);
}
} // namespace eidolon::ui
