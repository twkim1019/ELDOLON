#include "ParamPanel.h"

namespace eidolon::ui
{
std::vector<P> paramsInGroup (PG g)
{
    std::vector<P> v;
    for (int i = 0; i < kNumParams; ++i)
        if (paramInfo (i).group == g && (P) i != P::SceneAdvance) v.push_back ((P) i);
    return v;
}

ParamPanel::ParamPanel (EidolonCore& c, std::vector<Section> s) : core (c), sections (std::move (s))
{
    for (auto& sec : sections)
    {
        for (auto p : sec.params)
        {
            controls.push_back (std::make_unique<ParamControl> (core, p));
            content.addAndMakeVisible (*controls.back());
        }
        if (sec.custom != nullptr) content.addAndMakeVisible (sec.custom);
    }
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

void ParamPanel::refresh()
{
    for (auto& c : controls) c->refresh();
}

void ParamPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    const int w = getWidth() - 12;
    int y = 4;
    size_t ci = 0;
    content.headings.clear();
    for (auto& sec : sections)
    {
        if (sec.title.isNotEmpty())
        {
            content.headings.push_back ({ juce::Rectangle<float> (8.0f, (float) y + 8.0f, (float) w - 8.0f, 16.0f), sec.title });
            y += kHeading;
        }
        if (sec.custom != nullptr)
        {
            sec.custom->setBounds (8, y, w - 8, sec.customHeight);
            y += sec.customHeight + 4;
        }
        for (size_t k = 0; k < sec.params.size(); ++k, ++ci)
        {
            controls[ci]->setBounds (8, y, w - 8, kRow);
            y += kRow;
        }
        y += 6;
    }
    content.setBounds (0, 0, w, y + 12);
}

void ParamPanel::Content::paint (juce::Graphics& g)
{
    for (auto& h : headings) drawSectionHeading (g, h.first, h.second);
}
} // namespace eidolon::ui
