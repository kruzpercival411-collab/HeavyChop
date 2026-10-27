#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // note division -> length of one cycle in beats (quarter notes)
    const double divisionBeats[] = { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
    const double rampBarsValues[] = { 0.0, 0.5, 1.0, 2.0, 4.0, 8.0 };

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values; // real (denormalised) values
        const char* pattern; // 16 chars, x = on
    };

    // choice params use their index as the value
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list = {
            { "Init (Heavy Trem)", { { "shape", 0 }, { "rateStart", 4 }, { "rateEnd", 4 }, { "rampBars", 0 }, { "rampMode", 0 }, { "rampCurve", 0.5f },
                                     { "depth", 0.8f }, { "duty", 0.5f }, { "smooth", 3 }, { "stereo", 0 }, { "patLen", 16 },
                                     { "drive", 8 }, { "squash", 0.4f }, { "weight", 3 }, { "heavyMix", 1 }, { "width", 1.1f }, { "output", -3 } },
              "xxxxxxxxxxxxxxxx" },
            { "Flume Chop", { { "shape", 4 }, { "rateStart", 6 }, { "rateEnd", 6 }, { "rampBars", 0 }, { "rampMode", 0 }, { "rampCurve", 0.5f },
                              { "depth", 1 }, { "duty", 0.7f }, { "smooth", 1.5f }, { "stereo", 0 }, { "patLen", 16 },
                              { "drive", 12 }, { "squash", 0.65f }, { "weight", 4 }, { "heavyMix", 1 }, { "width", 1.25f }, { "output", -5 } },
              "x.xx..x.x.x..xx." },
            { "Slow -> Fast Build (4 bars)", { { "shape", 0 }, { "rateStart", 2 }, { "rateEnd", 9 }, { "rampBars", 4 }, { "rampMode", 0 }, { "rampCurve", 0.7f },
                                               { "depth", 1 }, { "duty", 0.5f }, { "smooth", 2 }, { "stereo", 0 }, { "patLen", 16 },
                                               { "drive", 10 }, { "squash", 0.5f }, { "weight", 5 }, { "heavyMix", 1 }, { "width", 1.2f }, { "output", -4 } },
              "xxxxxxxxxxxxxxxx" },
            { "Stutter Riser (2 bars)", { { "shape", 2 }, { "rateStart", 4 }, { "rateEnd", 10 }, { "rampBars", 3 }, { "rampMode", 0 }, { "rampCurve", 1.0f },
                                          { "depth", 1 }, { "duty", 0.45f }, { "smooth", 1 }, { "stereo", 0 }, { "patLen", 16 },
                                          { "drive", 16 }, { "squash", 0.7f }, { "weight", 4 }, { "heavyMix", 1 }, { "width", 1.0f }, { "output", -6 } },
              "xxxxxxxxxxxxxxxx" },
            { "Sidechain Pump", { { "shape", 3 }, { "rateStart", 2 }, { "rateEnd", 2 }, { "rampBars", 0 }, { "rampMode", 0 }, { "rampCurve", 0.5f },
                                  { "depth", 0.9f }, { "duty", 0.55f }, { "smooth", 1 }, { "stereo", 0 }, { "patLen", 16 },
                                  { "drive", 9 }, { "squash", 0.55f }, { "weight", 6 }, { "heavyMix", 1 }, { "width", 1.3f }, { "output", -4 } },
              "xxxxxxxxxxxxxxxx" },
            { "Wobble Ping-Pong", { { "shape", 1 }, { "rateStart", 4 }, { "rateEnd", 8 }, { "rampBars", 2 }, { "rampMode", 1 }, { "rampCurve", 0.5f },
                                    { "depth", 0.95f }, { "duty", 0.5f }, { "smooth", 2 }, { "stereo", 0.25f }, { "patLen", 16 },
                                    { "drive", 14 }, { "squash", 0.6f }, { "weight", 5 }, { "heavyMix", 0.85f }, { "width", 1.4f }, { "output", -5 } },
              "xxxxxxxxxxxxxxxx" },
            { "Brutal Gate", { { "shape", 4 }, { "rateStart", 6 }, { "rateEnd", 6 }, { "rampBars", 0 }, { "rampMode", 0 }, { "rampCurve", 0.5f },
                               { "depth", 1 }, { "duty", 0.85f }, { "smooth", 0.5f }, { "stereo", 0 }, { "patLen", 8 },
                               { "drive", 22 }, { "squash", 0.85f }, { "weight", 7 }, { "heavyMix", 1 }, { "width", 1.0f }, { "output", -8 } },
              "xx.x.xx.x......." },
        };
        return list;
    }
}

const juce::StringArray& HeavyChopProcessor::divisionNames()
{
    static const juce::StringArray names { "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout HeavyChopProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });

    // --- Motion ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape",
                                                       StringArray { "Sine", "Triangle", "Chop", "Pump", "Pattern" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rateStart", 1 }, "Rate (Start)", divisionNames(), 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rateEnd", 1 }, "Rate (End)", divisionNames(), 8));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rampBars", 1 }, "Ramp Length",
                                                       StringArray { "Off", "1/2 Bar", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rampMode", 1 }, "Ramp Mode",
                                                       StringArray { "Loop", "Ping-Pong", "MIDI Trigger" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "rampCurve", 1 }, "Ramp Curve", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Depth", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "duty", 1 }, "Gate Length", NormalisableRange<float> (0.05f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "smooth", 1 }, "Smooth",
                                                      NormalisableRange<float> (0.1f, 30.0f, 0.0f, 0.4f), 3.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "stereo", 1 }, "Stereo Offset", NormalisableRange<float> (0.0f, 0.5f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 360.0f)) + " deg"; })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "patLen", 1 }, "Pattern Length", 1, 16, 16));

    const char* defaultPattern = "x.xx..x.x.x..xx.";
    for (int i = 0; i < 16; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "step" + String (i + 1), 1 }, "Step " + String (i + 1), defaultPattern[i] == 'x'));

    // --- Heavy ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 30.0f), 8.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "squash", 1 }, "Squash", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "weight", 1 }, "Weight", NormalisableRange<float> (0.0f, 12.0f), 3.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "heavyMix", 1 }, "Heavy Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 2.0f), 1.1f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f), -3.0f, db));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "clip", 1 }, "Soft Clip", true));

    return layout;
}

HeavyChopProcessor::HeavyChopProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    p.shape = get ("shape");
    p.rateStart = get ("rateStart");
    p.rateEnd = get ("rateEnd");
    p.rampBars = get ("rampBars");
    p.rampMode = get ("rampMode");
    p.rampCurve = get ("rampCurve");
    p.depth = get ("depth");
    p.duty = get ("duty");
    p.smooth = get ("smooth");
    p.stereo = get ("stereo");
    p.patLen = get ("patLen");
    for (int i = 0; i < 16; ++i)
        p.steps[i] = get ("step" + juce::String (i + 1));
    p.drive = get ("drive");
    p.squash = get ("squash");
    p.weight = get ("weight");
    p.heavyMix = get ("heavyMix");
    p.width = get ("width");
    p.output = get ("output");
    p.clip = get ("clip");
}

bool HeavyChopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void HeavyChopProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    heavy.prepare (sampleRate, samplesPerBlock, channels);
    setLatencySamples (heavy.getLatencySamples());

    mod.prepare (sampleRate);
    gainState[0] = gainState[1] = 1.0f;
    freeRunPpq = 0.0;

    widthSmoothed.reset (sampleRate, 0.05);
    outputSmoothed.reset (sampleRate, 0.05);
    depthSmoothed.reset (sampleRate, 0.02);
    widthSmoothed.setCurrentAndTargetValue (p.width->load());
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.output->load()));
    depthSmoothed.setCurrentAndTargetValue (p.depth->load());
}

void HeavyChopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (2, getTotalNumOutputChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---------- timing ----------
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    double beatsPerBar = 4.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeSignature())
                beatsPerBar = ts->numerator * (4.0 / juce::jmax (1, ts->denominator));
        }
    }

    if (! playing)
        ppq = freeRunPpq;

    const double beatsPerSample = bpm / 60.0 / getSampleRate();

    // ---------- motion settings ----------
    const int shape = (int) p.shape->load();
    const int rampMode = (int) p.rampMode->load();

    ModEngine::Settings s;
    s.rateStart = 1.0 / divisionBeats[(int) p.rateStart->load()];
    s.rateEnd = 1.0 / divisionBeats[(int) p.rateEnd->load()];
    s.rampBeats = rampBarsValues[(int) p.rampBars->load()] * beatsPerBar;
    s.mode = rampMode;
    s.curveExp = std::pow (4.0, (p.rampCurve->load() - 0.5) * 2.0); // 0.25 .. 4
    mod.setSettings (s);

    if (playing && ! wasPlaying && rampMode == ModEngine::MidiTrigger)
        mod.retrigger();
    wasPlaying = playing;

    mod.syncToHost (playing, ppq);

    std::array<bool, 16> steps {};
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i] = p.steps[i]->load() > 0.5f;
    const int patLen = (int) p.patLen->load();
    const float duty = p.duty->load();
    const float stereoOffset = p.stereo->load();
    const float smoothCoef = 1.0f - std::exp (-1.0f / (float) (getSampleRate() * p.smooth->load() * 0.001));

    depthSmoothed.setTargetValue (p.depth->load());
    widthSmoothed.setTargetValue (p.width->load());
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p.output->load()));

    // ---------- heavy section ----------
    heavy.setParameters (p.drive->load(), p.squash->load(), p.weight->load(), p.heavyMix->load());
    heavy.process (buffer);

    // ---------- motion + width + output ----------
    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool clip = p.clip->load() > 0.5f;
    // MIDI note-on retriggers the ramp in MIDI Trigger mode (sample accurate)
    auto midiIt = midi.cbegin();
    const bool useMidi = rampMode == ModEngine::MidiTrigger;

    for (int i = 0; i < numSamples; ++i)
    {
        for (; useMidi && midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
            if ((*midiIt).getMessage().isNoteOn())
                mod.retrigger();

        const double c = mod.getCycles();
        const float depth = depthSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double cc = c + (ch == 1 ? stereoOffset : 0.0f);
            const float lfo = ModEngine::shapeValue (shape, cc, duty, steps, patLen);
            const float target = 1.0f - depth * (1.0f - lfo);
            gainState[ch] += (target - gainState[ch]) * smoothCoef;
        }

        float l = left[i] * gainState[0];
        float r = right != nullptr ? right[i] * gainState[1] : 0.0f;

        if (right != nullptr)
        {
            const float w = widthSmoothed.getNextValue();
            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * w;
            l = mid + side;
            r = mid - side;
        }

        const float out = outputSmoothed.getNextValue();
        l *= out;
        r *= out;

        if (clip)
        {
            // cubic soft clip, transparent below ~ -6 dBFS, ceiling at 1.0
            auto sc = [] (float x) { x = juce::jlimit (-1.5f, 1.5f, x); return x - (4.0f / 27.0f) * x * x * x; };
            l = sc (l);
            r = sc (r);
        }

        left[i] = l;
        if (right != nullptr)
            right[i] = r;

        mod.advance (beatsPerSample);
    }

    if (! playing)
        freeRunPpq += beatsPerSample * numSamples;

    displayGain.store (gainState[0]);
    displayStep.store (ModEngine::stepIndex (mod.getCycles(), patLen));
    displayRate.store ((float) mod.getCurrentRate());
}

int HeavyChopProcessor::getNumPrograms()
{
    return (int) presets().size();
}

const juce::String HeavyChopProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? presets()[(size_t) index].name : "";
}

void HeavyChopProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;

    currentProgram = index;
    const auto& preset = presets()[(size_t) index];

    for (const auto& [id, value] : preset.values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));

    for (int i = 0; i < 16; ++i)
        if (auto* param = apvts.getParameter ("step" + juce::String (i + 1)))
            param->setValueNotifyingHost (preset.pattern[i] == 'x' ? 1.0f : 0.0f);
}

void HeavyChopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HeavyChopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HeavyChopProcessor::createEditor()
{
    return new HeavyChopEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeavyChopProcessor();
}
