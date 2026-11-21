#pragma once
// EIDOLON — the main canvas. Renders the visualization engine; GPU accelerated through an attached
// juce::OpenGLContext when available (Windows build), with automatic fallback to the CPU renderer.
#include "Theme.h"
#include "../Engine/EidolonCore.h"
#if EIDOLON_OPENGL
 #include <juce_opengl/juce_opengl.h>
#endif

namespace eidolon::ui
{
struct DashboardState
{
    double lastUpdate = -1;
    float velocity = 0, density = 0, onset = 0, chordConf = 0, scaleConf = 0, polyphony = 0;
    juce::String chord, chordType, transition, scale, notes, tempo;
};

void drawDashboard (juce::Graphics& g, juce::Rectangle<float> canvas, EidolonCore& core, DashboardState& st);

class VisualizationCanvas : public juce::Component
{
public:
    VisualizationCanvas (EidolonCore& core, bool primary);
    ~VisualizationCanvas() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Called after each engine tick. Repaints and runs the GPU watchdog. */
    void frame();
    void setPaused (bool p) { paused = p; repaint(); }
    juce::String rendererName() const;
    bool gpuActive() const;

private:
    void updateGpu();
    EidolonCore& core;
    bool primary;
    bool paused = false;
    DashboardState dash;
    juce::Point<float> last;
    bool panning = false;
#if EIDOLON_OPENGL
    juce::OpenGLContext gl;
    bool glAttached = false, glFailed = false;
    std::atomic<bool> glFrameSeen { false };
    double glAttachTime = 0;
    bool glMsaa = true;
#endif
};
} // namespace eidolon::ui
