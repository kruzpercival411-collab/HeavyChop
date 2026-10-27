#pragma once

#include <juce_dsp/juce_dsp.h>

// The "thick and heavy" section:
//   oversampled asymmetric saturation -> 3-band upward/downward compressor (OTT-style)
//   -> low-shelf "weight", all blended in parallel with the dry signal.
class HeavyChain
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;
        channels = juce::jlimit (1, 2, numChannels);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlockSize);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) channels };

        for (auto* f : { &lowSplit, &highSplit })
            f->prepare (spec);
        lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        lowSplit.setCutoffFrequency (120.0f);
        highSplit.setCutoffFrequency (2500.0f);

        restSplit.prepare (spec);
        restSplit.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        restSplit.setCutoffFrequency (120.0f);

        // attack / release per band (ms), low band slower
        const float att[3] = { 6.0f, 3.0f, 1.5f };
        const float rel[3] = { 140.0f, 90.0f, 50.0f };
        for (int b = 0; b < 3; ++b)
        {
            bands[b].attCoef = coefFromMs (att[b]);
            bands[b].relCoef = coefFromMs (rel[b]);
            bands[b].env = 0.0f;
        }

        for (int b = 0; b < 3; ++b)
            bandBuffers[b].setSize (channels, maxBlockSize);

        for (auto& f : shelf)
        {
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, 90.0, 0.7f, 1.0f);
            f.reset();
        }
        lastWeight = 0.0f;

        mixer.prepare (spec);
        mixer.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
        mixer.setWetLatency (oversampler->getLatencyInSamples());

        driveSmoothed.reset (sampleRate * 2.0, 0.03);
        squashSmoothed.reset (sampleRate, 0.03);
    }

    int getLatencySamples() const
    {
        return oversampler ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    }

    void setParameters (float driveDb, float squash, float weightDb, float mix)
    {
        driveSmoothed.setTargetValue (driveDb);
        squashSmoothed.setTargetValue (squash);
        mixer.setWetMixProportion (mix);

        if (std::abs (weightDb - lastWeight) > 0.01f)
        {
            lastWeight = weightDb;
            auto coefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                sr, 90.0, 0.7f, juce::Decibels::decibelsToGain (weightDb));
            for (auto& f : shelf)
                *f.coefficients = *coefs;
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) channels, (size_t) numSamples);

        mixer.pushDrySamples (block);

        processSaturation (block);
        processMultiband (buffer, numSamples);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = shelf[ch].processSample (d[i]);
        }

        mixer.mixWetSamples (block);
    }

private:
    struct Band
    {
        float env = 0.0f, attCoef = 0.0f, relCoef = 0.0f;
    };

    float coefFromMs (float ms) const
    {
        return 1.0f - std::exp (-1.0f / (float) (sr * ms * 0.001));
    }

    void processSaturation (juce::dsp::AudioBlock<float>& block)
    {
        auto up = oversampler->processSamplesUp (block);
        const int n = (int) up.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const float driveDb = driveSmoothed.getNextValue();
            const float k = juce::Decibels::decibelsToGain (driveDb);
            // asymmetry grows with drive -> even harmonics = "thick"
            const float bias = 0.25f * (driveDb / 30.0f);
            const float tb = std::tanh (bias);
            const float norm = 1.0f / (std::tanh (k) * (1.0f - tb * tb) + 1.0e-6f);

            for (int ch = 0; ch < (int) up.getNumChannels(); ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                d[i] = (std::tanh (k * d[i] + bias) - tb) * norm;
            }
        }

        oversampler->processSamplesDown (block);
    }

    void processMultiband (juce::AudioBuffer<float>& buffer, int numSamples)
    {
        // Split into low / mid / high (phase-aligned LR4)
        for (int ch = 0; ch < channels; ++ch)
        {
            const float* in = buffer.getReadPointer (ch);
            float* lo = bandBuffers[0].getWritePointer (ch);
            float* mid = bandBuffers[1].getWritePointer (ch);
            float* hi = bandBuffers[2].getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = in[i];
                const float rest = restSplit.processSample (ch, x);
                const float m = highSplit.processSample (ch, rest);
                lo[i] = lowSplit.processSample (ch, x);
                mid[i] = m;
                hi[i] = rest - m; // complementary split so mid + high == rest exactly
            }
        }

        // Upward + downward compression per band, stereo linked
        constexpr float downThr = -24.0f, downRatio = 6.0f;
        constexpr float upThr = -42.0f, upRatio = 3.0f, maxUp = 18.0f;
        constexpr float makeup = 7.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float amount = squashSmoothed.getNextValue();

            for (int b = 0; b < 3; ++b)
            {
                auto& band = bands[b];
                float level = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    level = std::max (level, std::abs (bandBuffers[b].getSample (ch, i)));

                const float coef = level > band.env ? band.attCoef : band.relCoef;
                band.env += (level - band.env) * coef;

                const float db = juce::Decibels::gainToDecibels (band.env, -100.0f);
                float gr = 0.0f;
                if (db > downThr)
                    gr = -(db - downThr) * (1.0f - 1.0f / downRatio);
                else if (db < upThr)
                    gr = std::min (maxUp, (upThr - db) * (1.0f - 1.0f / upRatio))
                         * juce::jlimit (0.0f, 1.0f, (db + 75.0f) / 15.0f); // don't lift the noise floor

                const float g = juce::Decibels::decibelsToGain ((gr + makeup) * amount);
                for (int ch = 0; ch < channels; ++ch)
                    bandBuffers[b].getWritePointer (ch)[i] *= g;
            }
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                out[i] = bandBuffers[0].getSample (ch, i) + bandBuffers[1].getSample (ch, i)
                       + bandBuffers[2].getSample (ch, i);
        }
    }

    double sr = 44100.0;
    int channels = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, restSplit, highSplit;
    juce::AudioBuffer<float> bandBuffers[3];
    Band bands[3];

    juce::dsp::IIR::Filter<float> shelf[2];
    float lastWeight = -1.0f;

    juce::dsp::DryWetMixer<float> mixer { 64 };
    juce::SmoothedValue<float> driveSmoothed, squashSmoothed;
};
