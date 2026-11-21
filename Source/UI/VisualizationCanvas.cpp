#include "VisualizationCanvas.h"
#include "../Scenes/SceneManager.h"

namespace eidolon::ui
{
VisualizationCanvas::VisualizationCanvas (EidolonCore& c, bool isPrimary) : core (c), primary (isPrimary)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
    updateGpu();
}

VisualizationCanvas::~VisualizationCanvas()
{
#if EIDOLON_OPENGL
    if (glAttached) gl.detach();
#endif
}

void VisualizationCanvas::updateGpu()
{
#if EIDOLON_OPENGL
    const bool want = core.params().getb (P::RenderGpu) && ! glFailed;
    const bool msaa = core.params().getb (P::RenderAA);
    if (glAttached && (! want || msaa != glMsaa)) { gl.detach(); glAttached = false; }
    if (want && ! glAttached)
    {
        juce::OpenGLPixelFormat pf;
        pf.multisamplingLevel = msaa ? 4 : 0;
        gl.setPixelFormat (pf);
        gl.setMultisamplingEnabled (msaa);
        gl.setComponentPaintingEnabled (true);
        gl.setContinuousRepainting (false);
        glFrameSeen = false;
        gl.attachTo (*this);
        glAttached = true;
        glMsaa = msaa;
        glAttachTime = juce::Time::getMillisecondCounterHiRes();
    }
#endif
}

bool VisualizationCanvas::gpuActive() const
{
#if EIDOLON_OPENGL
    return glAttached && glFrameSeen.load();
#else
    return false;
#endif
}

juce::String VisualizationCanvas::rendererName() const
{
#if EIDOLON_OPENGL
    if (glFailed) return "CPU (GPU unavailable)";
    if (glAttached) return glFrameSeen.load() ? "GPU (OpenGL)" : "GPU (starting)";
    return "CPU";
#else
    return "CPU (this build has no OpenGL)";
#endif
}

void VisualizationCanvas::frame()
{
#if EIDOLON_OPENGL
    updateGpu();
    // watchdog: if the GPU context never produced a frame, fall back to the CPU renderer
    if (glAttached && ! glFrameSeen.load() && isShowing() && juce::Time::getMillisecondCounterHiRes() - glAttachTime > 4000.0)
    {
        gl.detach();
        glAttached = false;
        glFailed = true;
    }
#endif
    if (isShowing() && ! paused) repaint();
}

void VisualizationCanvas::paint (juce::Graphics& g)
{
#if EIDOLON_OPENGL
    if (juce::OpenGLContext::getCurrentContext() != nullptr) glFrameSeen = true;
#endif
    const auto area = getLocalBounds().toFloat();
    if (paused)
    {
        g.fillAll (colours::bg0);
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (13.0f));
        g.drawText ("The visualizer is open in its own window", area, juce::Justification::centred);
        return;
    }
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    core.visuals().render (g, area, gpuActive());
    const auto& p = core.params();

    if (p.getb (P::DashOn)) drawDashboard (g, area, core, dash);

    if (! core.midiEverReceived())
    {
        auto r = area.withSizeKeepingCentre (std::min (520.0f, area.getWidth() - 40.0f), 86.0f).translated (0, area.getHeight() * 0.18f);
        g.setColour (colours::bg1.withAlpha (0.72f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::line2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (colours::text);
        g.setFont (Fonts::get().medium (13.5f));
        g.drawText ("Waiting for MIDI", r.removeFromTop (34.0f).translated (0, 6), juce::Justification::centred);
        g.setColour (colours::text2);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawFittedText ("No MIDI has reached EIDOLON yet. In FL Studio, load it as a Channel Rack instrument\n"
                          "(or route a MIDI Out port to it) and play the Piano Roll or a controller.",
                          r.reduced (14.0f, 0).toNearestInt(), juce::Justification::centredTop, 3);
    }

    if (p.getb (P::RenderPerf))
    {
        const auto& s = core.visuals().stats();
        juce::StringArray lines;
        lines.add (rendererName());
        lines.add (juce::String (s.fps, 1) + " fps   " + juce::String (s.renderMs, 1) + " ms render   " + juce::String (s.updateMs, 2) + " ms update");
        lines.add ("quality " + juce::String ((int) std::lround (s.quality * 100)) + "%   notes " + juce::String (s.notes) + "   particles " + juce::String (s.particles));
        lines.add ("events " + juce::String ((juce::int64) core.eventsReceived()) + "   dropped " + juce::String ((juce::int64) core.eventsDropped()));
        auto r = juce::Rectangle<float> (area.getRight() - 300.0f, area.getY() + 10.0f, 290.0f, 14.0f * (float) lines.size() + 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setFont (Fonts::get().mono (10.5f));
        g.setColour (colours::text2);
        for (int i = 0; i < lines.size(); ++i) g.drawText (lines[i], r.reduced (8.0f, 5.0f).withHeight (14.0f).translated (0, 14.0f * (float) i), juce::Justification::centredLeft);
    }

    if (primary) core.visuals().reportFrameTime ((float) (juce::Time::getMillisecondCounterHiRes() - t0));
}

void VisualizationCanvas::mouseDown (const juce::MouseEvent& e)
{
    last = e.position;
    panning = e.mods.isRightButtonDown() || e.mods.isShiftDown() || e.mods.isMiddleButtonDown();
}

void VisualizationCanvas::mouseDrag (const juce::MouseEvent& e)
{
    const auto d = e.position - last;
    last = e.position;
    if (panning) core.visuals().cameraController().pan (d.x, d.y, core.visuals().camera());
    else core.visuals().cameraController().drag (d.x, d.y);
}

void VisualizationCanvas::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    core.visuals().cameraController().zoom (w.deltaY);
}

void VisualizationCanvas::mouseDoubleClick (const juce::MouseEvent&) { core.visuals().cameraController().reset(); }

} // namespace eidolon::ui
