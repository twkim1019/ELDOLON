#pragma once
// EIDOLON — scrollable configuration panel built from sections. A section is either a list of parameters
// (rows generated from the parameter table) or a custom component with a fixed height.
#include "Controls.h"
#include <memory>
#include <vector>

namespace eidolon::ui
{
std::vector<P> paramsInGroup (PG g);

class ParamPanel : public juce::Component
{
public:
    struct Section
    {
        juce::String title;
        std::vector<P> params;
        juce::Component* custom = nullptr;     // not owned
        int customHeight = 0;
    };

    ParamPanel (EidolonCore& core, std::vector<Section> sections);
    void refresh();
    void resized() override;
    static constexpr int kRow = 28, kHeading = 30;

private:
    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        std::vector<std::pair<juce::Rectangle<float>, juce::String>> headings;
    };
    EidolonCore& core;
    std::vector<Section> sections;
    juce::Viewport viewport;
    Content content;
    std::vector<std::unique_ptr<ParamControl>> controls;
};
} // namespace eidolon::ui
