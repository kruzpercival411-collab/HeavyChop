#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class HeavyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HeavyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Scrolling display of the modulation gain
class GainScope : public juce::Component
{
public:
    void push (float v);
    void paint (juce::Graphics&) override;

private:
    std::array<float, 240> history {};
    int writePos = 0;
};

class HeavyChopEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HeavyChopEditor (HeavyChopProcessor&);
    ~HeavyChopEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupChoice (Choice&, const juce::String& paramId, const juce::String& name);

    HeavyChopProcessor& processor;
    HeavyLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::Label rateLabel;

    Choice shape, rateStart, rateEnd, rampBars, rampMode;
    Knob rampCurve, depth, duty, smooth, stereo, patLen;
    Knob drive, squash, weight, heavyMix, width, output;

    juce::TextButton steps[16];
    std::unique_ptr<ButtonAttachment> stepAttachments[16];
    int lastStep = -1;

    juce::ToggleButton clip { "Soft Clip" };
    std::unique_ptr<ButtonAttachment> clipAttachment;

    GainScope scope;

    juce::Rectangle<int> motionArea, heavyArea, patternArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavyChopEditor)
};
