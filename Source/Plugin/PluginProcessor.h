#pragma once
// EIDOLON — MIDI Universe Visualizer: audio processor.
// The audio callback only converts MIDI to POD events and pushes them into a bounded lock-free queue,
// samples the host transport into atomics and outputs silence. No allocation, no locks, no file I/O,
// no analysis and no rendering happen on the audio thread.
#include "../Engine/EidolonCore.h"
#include <juce_audio_processors/juce_audio_processors.h>

class EidolonProcessor : public juce::AudioProcessor
{
public:
    EidolonProcessor();
    ~EidolonProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EIDOLON"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    eidolon::EidolonCore& core() { return *engine; }
    eidolon::AudioThreadShared& shared() { return audioShared; }
    juce::AudioProcessorValueTreeState& hostParameters() { return apvts; }

private:
    template <typename Sample> void process (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    eidolon::AudioThreadShared audioShared;
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<eidolon::EidolonCore> engine;
    std::atomic<float>* pIntensity = nullptr;
    std::atomic<float>* pMotion = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pScene = nullptr;
    std::atomic<float>* pNext = nullptr;
    double sampleRate = 48000.0;
    juce::int64 samplesProcessed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EidolonProcessor)
};
