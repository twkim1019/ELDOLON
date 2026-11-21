#include "PluginProcessor.h"
#include "../DNA/FactoryPresets.h"
#include "../Scenes/SceneManager.h"
#include "../UI/PluginEditor.h"

using namespace eidolon;

juce::AudioProcessorValueTreeState::ParameterLayout EidolonProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "intensity", 1 }, "Visual Intensity", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "motion", 1 }, "Motion Speed", NormalisableRange<float> (0.1f, 3.0f, 0.001f, 0.6f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release Scale", NormalisableRange<float> (0.25f, 4.0f, 0.001f, 0.5f), 1.0f));
    StringArray scenes;
    for (int i = 0; i < kNumScenes; ++i) scenes.add (sceneName (i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scene", 1 }, "Scene", scenes, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "nextScene", 1 }, "Next Scene", false));
    return layout;
}

EidolonProcessor::EidolonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EIDOLON_HOST", createLayout())
{
    pIntensity = apvts.getRawParameterValue ("intensity");
    pMotion = apvts.getRawParameterValue ("motion");
    pRelease = apvts.getRawParameterValue ("release");
    pScene = apvts.getRawParameterValue ("scene");
    pNext = apvts.getRawParameterValue ("nextScene");
    engine = std::make_unique<EidolonCore> (audioShared);
}

EidolonProcessor::~EidolonProcessor() { engine.reset(); }

bool EidolonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.inputBuses.isEmpty() || layouts.getMainInputChannelSet().isDisabled() || layouts.getMainInputChannelSet() == out;
}

void EidolonProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0 ? sr : 48000.0;
}

void EidolonProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { process (b, m); }
void EidolonProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { process (b, m); }

template <typename Sample>
void EidolonProcessor::process (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double blockStart = (double) samplesProcessed / sampleRate;
    auto& s = audioShared;

    // ---- host transport (all fields optional in JUCE 8)
    bool valid = false, playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { s.bpm.store (*bpm); valid = true; }
            if (auto ppq = pos->getPpqPosition()) s.ppq.store (*ppq); else valid = false;
            if (auto bar = pos->getPpqPositionOfLastBarStart()) s.barStartPpq.store (*bar);
            if (auto sig = pos->getTimeSignature()) { s.numerator.store (sig->numerator); s.denominator.store (sig->denominator); }
            s.transportTime.store (blockStart);
        }
    s.transportValid.store (valid);
    s.playing.store (playing);

    // ---- MIDI -> lock-free queue (bounded; continuous controllers are thinned first when it fills up)
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        MidiEvent e;
        e.time = blockStart + (double) meta.samplePosition / sampleRate;
        e.channel = (uint8_t) juce::jlimit (0, 15, msg.getChannel() - 1);
        if (msg.isNoteOn())                     { e.type = EvType::NoteOn; e.data1 = (uint8_t) msg.getNoteNumber(); e.data2 = msg.getVelocity(); }
        else if (msg.isNoteOff())               { e.type = EvType::NoteOff; e.data1 = (uint8_t) msg.getNoteNumber(); e.data2 = msg.getVelocity(); }
        else if (msg.isController())            { e.type = EvType::Controller; e.data1 = (uint8_t) msg.getControllerNumber(); e.data2 = (uint8_t) msg.getControllerValue(); }
        else if (msg.isPitchWheel())            { e.type = EvType::PitchBend; e.value = (int16_t) (msg.getPitchWheelValue() - 8192); }
        else if (msg.isChannelPressure())       { e.type = EvType::ChannelPressure; e.data2 = (uint8_t) msg.getChannelPressureValue(); }
        else if (msg.isAftertouch())            { e.type = EvType::PolyAftertouch; e.data1 = (uint8_t) msg.getNoteNumber(); e.data2 = (uint8_t) msg.getAfterTouchValue(); }
        else if (msg.isProgramChange())         { e.type = EvType::ProgramChange; e.data1 = (uint8_t) msg.getProgramChangeNumber(); }
        else if (msg.isMidiClock())             { e.type = EvType::Clock; }
        else if (msg.isMidiStart())             { e.type = EvType::Start; }
        else if (msg.isMidiStop())              { e.type = EvType::Stop; }
        else if (msg.isMidiContinue())          { e.type = EvType::Continue; }
        else if (msg.isSongPositionPointer())   { e.type = EvType::SongPosition; e.value = (int16_t) msg.getSongPositionPointerMidiBeat(); }
        else continue;   // sysex / other system messages are not visualized

        s.received.fetch_add (1, std::memory_order_relaxed);
        if (e.isContinuous() && s.queue.size() > MidiQueue::capacity() * 3 / 4)
        {
            s.thinned.fetch_add (1, std::memory_order_relaxed);   // keep room for notes
            continue;
        }
        if (! s.queue.push (e)) s.dropped.fetch_add (1, std::memory_order_relaxed);
    }

    samplesProcessed += n;
    s.blockEndTime.store ((double) samplesProcessed / sampleRate);
    s.hostIntensity.store (pIntensity->load());
    s.hostMotion.store (pMotion->load());
    s.hostRelease.store (pRelease->load());
    s.hostScene.store ((int) pScene->load());
    s.hostNextScene.store (pNext->load() >= 0.5f);

    if (! s.midiThru.load()) midi.clear();
    buffer.clear();   // a visualizer: never alters or produces audio
}

juce::AudioProcessorEditor* EidolonProcessor::createEditor() { return new EidolonEditor (*this); }

int EidolonProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
int EidolonProcessor::getCurrentProgram() { return engine->currentProgram(); }
void EidolonProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < getNumPrograms() && index != engine->currentProgram()) engine->requestProgram (index);
}
const juce::String EidolonProcessor::getProgramName (int index)
{
    const auto& fp = factoryPresets();
    return index >= 0 && index < (int) fp.size() ? juce::String (fp[(size_t) index].name) : juce::String();
}

void EidolonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr && juce::MessageManager::getInstance()->isThisTheMessageThread())
        engine->flushStateIfDirty();
    juce::ValueTree root ("EIDOLON");
    root.setProperty ("stateVersion", 1, nullptr);
    root.setProperty ("engine", engine->getStateJson(), nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void EidolonProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("EIDOLON")) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    const auto host = root.getChildWithName (apvts.state.getType());
    if (host.isValid()) apvts.replaceState (host);
    const auto json = root.getProperty ("engine").toString();
    if (json.isNotEmpty()) engine->restoreStateJson (json);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EidolonProcessor(); }
