// EIDOLON — capture: start / arm / stop, internal replay, Standard MIDI File export (file chooser or drag
// the take out of the plugin), and a timeline view for inspecting the captured MIDI.
#include "Panels.h"

namespace eidolon::ui
{
CaptureHeader::CaptureHeader (EidolonCore& c) : core (c)
{
    for (auto* b : { &startButton, &stopButton, &replayButton, &exportButton, &clearButton }) addAndMakeVisible (b);
    startButton.setTooltip ("Fixed: start recording now. Triggered / Phrase: arm and wait for the first note. Live buffer / Loop: restart the buffer.");
    stopButton.setTooltip ("Finish the take. In Live Buffer mode this captures the last N seconds.");
    replayButton.setTooltip ("Replay the take internally through the visualizer (MIDI is not sent to the host)");
    exportButton.setTooltip ("Save the take as a Standard MIDI File (.mid)");
    startButton.onClick = [this] { core.configureCapture(); core.capture().start (core.engineNow()); message = {}; };
    stopButton.onClick = [this] { core.capture().stop (core.engineNow()); };
    replayButton.onClick = [this]
    {
        if (core.capture().isReplaying()) core.capture().stopReplay();
        else if (core.capture().lastTake().empty()) message = "Nothing to replay yet.";
        else core.capture().startReplay (core.engineNow(), core.params().getb (P::CapReplayLoop));
    };
    clearButton.onClick = [this] { core.capture().clearTake(); message = {}; };
    exportButton.onClick = [this]
    {
        if (core.capture().lastTake().empty()) { message = "No take to export yet."; return; }
        const auto initial = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                 .getChildFile ("EIDOLON_capture_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".mid");
        chooser = std::make_unique<juce::FileChooser> ("Export capture as MIDI", initial, "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  juce::String err;
                                  message = core.exportTake (f.withFileExtension ("mid"), err) ? "Saved " + f.withFileExtension ("mid").getFileName() : err;
                              });
    };
    startTimerHz (12);
}

void CaptureHeader::timerCallback()
{
    replayButton.setButtonText (core.capture().isReplaying() ? "Stop replay" : "Replay");
    repaint();
}

void CaptureHeader::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (24);
    auto row = r.removeFromTop (26);
    const int bw = (row.getWidth() - 12) / 3;
    startButton.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (6);
    stopButton.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (6);
    replayButton.setBounds (row);
    r.removeFromTop (6);
    row = r.removeFromTop (26);
    exportButton.setBounds (row.removeFromLeft (bw * 2 + 6)); row.removeFromLeft (6);
    clearButton.setBounds (row);
    r.removeFromTop (6);
    dragArea = r.removeFromTop (30);
    r.removeFromTop (8);
    timelineArea = r.removeFromTop (std::max (60, r.getHeight() - 22));
}

void CaptureHeader::paint (juce::Graphics& g)
{
    const auto& cap = core.capture();
    const double now = core.engineNow();
    g.setColour (cap.state() == CaptureState::Recording ? colours::warn : cap.state() == CaptureState::Armed ? colours::warm : colours::text2);
    g.setFont (Fonts::get().medium (12.5f));
    g.drawText (juce::String (cap.statusText (now)), getLocalBounds().removeFromTop (20), juce::Justification::centredLeft, true);

    // drag handle
    const bool hasTake = ! cap.lastTake().empty();
    g.setColour (hasTake ? colours::bg3.brighter (0.05f) : colours::bg2);
    g.fillRoundedRectangle (dragArea.toFloat(), 4.0f);
    g.setColour (hasTake ? colours::accent.withAlpha (0.5f) : colours::line2);
    g.drawRoundedRectangle (dragArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
    drawIcon (g, Icon::Drag, dragArea.toFloat().removeFromLeft (30).withSizeKeepingCentre (18, 18), hasTake ? colours::accent : colours::text3);
    g.setColour (hasTake ? colours::text : colours::text3);
    g.setFont (Fonts::get().regular (12.0f));
    g.drawText (hasTake ? "Drag this take into your DAW as a .mid file" : "Drag a take out of here once captured", dragArea.withTrimmedLeft (30), juce::Justification::centredLeft, true);

    // timeline
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (timelineArea.toFloat(), 4.0f);
    const auto& take = cap.lastTake();
    auto tl = timelineArea.toFloat().reduced (6.0f, 6.0f);
    if (take.empty())
    {
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText ("No take captured", tl, juce::Justification::centred);
    }
    else
    {
        int lo = 127, hi = 0;
        for (const auto& e : take.events) if (e.type == EvType::NoteOn && e.data2 > 0) { lo = std::min (lo, (int) e.data1); hi = std::max (hi, (int) e.data1); }
        if (lo > hi) { lo = 48; hi = 72; }
        lo = std::max (0, lo - 2); hi = std::min (127, hi + 2);
        const float len = (float) std::max (0.5, take.length);
        const auto lanes = tl.removeFromBottom (14.0f);
        std::array<std::array<float, 128>, 16> onAt;
        for (auto& ch : onAt) ch.fill (-1.0f);
        auto xOf = [&] (double t) { return tl.getX() + (float) t / len * tl.getWidth(); };
        auto yOf = [&] (int p) { return tl.getBottom() - (float) (p - lo + 1) / (float) (hi - lo + 1) * tl.getHeight(); };
        const float rowH = std::max (1.5f, tl.getHeight() / (float) (hi - lo + 1) - 0.5f);
        auto drawNote = [&] (int ch, int p, float t0, float t1)
        {
            g.setColour (juce::Colour::fromHSV ((float) ((p * 7) % 12) / 12.0f, 0.45f, 0.95f, 0.85f));
            g.fillRect (xOf (t0), yOf (p), std::max (1.5f, xOf (t1) - xOf (t0)), rowH);
            juce::ignoreUnused (ch);
        };
        for (const auto& e : take.events)
        {
            const int ch = e.channel & 15;
            if (e.type == EvType::NoteOn && e.data2 > 0) onAt[(size_t) ch][e.data1] = (float) e.time;
            else if (e.type == EvType::NoteOff || (e.type == EvType::NoteOn && e.data2 == 0))
            {
                if (onAt[(size_t) ch][e.data1] >= 0) drawNote (ch, e.data1, onAt[(size_t) ch][e.data1], (float) e.time);
                onAt[(size_t) ch][e.data1] = -1;
            }
            else if (e.type == EvType::Controller)
            {
                g.setColour (colours::accent2.withAlpha (0.6f));
                g.fillRect (xOf (e.time), lanes.getBottom() - (float) e.data2 / 127.0f * lanes.getHeight(), 1.0f, (float) e.data2 / 127.0f * lanes.getHeight());
            }
            else if (e.type == EvType::PitchBend)
            {
                g.setColour (colours::warm.withAlpha (0.7f));
                g.fillRect (xOf (e.time), lanes.getCentreY() - (float) e.value / 8192.0f * lanes.getHeight() * 0.5f, 1.5f, 1.5f);
            }
        }
        for (int ch = 0; ch < 16; ++ch) for (int p = 0; p < 128; ++p) if (onAt[(size_t) ch][(size_t) p] >= 0) drawNote (ch, p, onAt[(size_t) ch][(size_t) p], len);
        if (cap.isReplaying())
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            const float px = tl.getX() + cap.replayProgress (now) * tl.getWidth();
            g.drawVerticalLine ((int) px, tl.getY(), lanes.getBottom());
        }
        g.setColour (colours::text3);
        g.setFont (Fonts::get().mono (10.0f));
        g.drawText (juce::String (take.label), timelineArea.toFloat().reduced (8, 4), juce::Justification::topRight, true);
    }
    g.setColour (colours::accent);
    g.setFont (Fonts::get().regular (11.5f));
    g.drawText (message, getLocalBounds().removeFromBottom (18), juce::Justification::centredLeft, true);
}

void CaptureHeader::mouseDown (const juce::MouseEvent&) { dragging = false; }

void CaptureHeader::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! dragArea.contains (e.getMouseDownPosition()) || e.getDistanceFromDragStart() < 6) return;
    if (core.capture().lastTake().empty()) { message = "Capture a take first."; return; }
    const auto f = core.writeTakeToTempFile();
    if (! f.existsAsFile()) { message = "Could not write temporary MIDI file."; return; }
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
}
} // namespace eidolon::ui
