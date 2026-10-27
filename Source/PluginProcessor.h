#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ModEngine.h"
#include "HeavyChain.h"

class HeavyChopProcessor : public juce::AudioProcessor
{
public:
    HeavyChopProcessor();
    ~HeavyChopProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& divisionNames();

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor for the scope / step display
    std::atomic<float> displayGain { 1.0f };
    std::atomic<int> displayStep { 0 };
    std::atomic<float> displayRate { 1.0f };

private:
    ModEngine mod;
    HeavyChain heavy;

    float gainState[2] { 1.0f, 1.0f };
    juce::SmoothedValue<float> widthSmoothed, outputSmoothed, depthSmoothed;
    double freeRunPpq = 0.0;
    bool wasPlaying = false;
    int currentProgram = 0;

    struct Params
    {
        std::atomic<float>* shape;
        std::atomic<float>* rateStart;
        std::atomic<float>* rateEnd;
        std::atomic<float>* rampBars;
        std::atomic<float>* rampMode;
        std::atomic<float>* rampCurve;
        std::atomic<float>* depth;
        std::atomic<float>* duty;
        std::atomic<float>* smooth;
        std::atomic<float>* stereo;
        std::atomic<float>* patLen;
        std::atomic<float>* steps[16];
        std::atomic<float>* drive;
        std::atomic<float>* squash;
        std::atomic<float>* weight;
        std::atomic<float>* heavyMix;
        std::atomic<float>* width;
        std::atomic<float>* output;
        std::atomic<float>* clip;
    } p;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavyChopProcessor)
};
