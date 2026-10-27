// Offline render test: runs every preset over a sine and reports level stats.
#include "../Source/PluginProcessor.h"
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 512;
    int failures = 0;

    for (int preset = 0; preset < 7; ++preset)
    {
        HeavyChopProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.setCurrentProgram (preset);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        const int total = (int) (sr * 8.0);
        std::vector<float> outL;
        double phase = 0.0;
        bool bad = false;
        float peak = 0.0f;

        for (int pos = 0; pos < total; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const float x = 0.5f * (float) std::sin (phase);
                phase += 2.0 * juce::MathConstants<double>::pi * 110.0 / sr;
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            proc.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                if (! std::isfinite (l) || ! std::isfinite (r)) bad = true;
                peak = std::max ({ peak, std::abs (l), std::abs (r) });
                outL.push_back (l);
            }
        }

        // RMS per 1/16 note (120 bpm free-run -> 6000 samples) for the first bar
        printf ("%-28s peak %.3f %s\n  1/16 RMS bar1: ", proc.getProgramName (preset).toRawUTF8(), peak, bad ? "NaN!" : "");
        for (int s = 0; s < 16; ++s)
        {
            double acc = 0;
            for (int i = s * 6000; i < (s + 1) * 6000; ++i) acc += outL[(size_t) i] * outL[(size_t) i];
            printf ("%.2f ", std::sqrt (acc / 6000));
        }
        printf ("\n");
        if (bad || peak > 1.0001f) ++failures;
    }
    printf (failures ? "FAILED\n" : "ALL OK\n");
    return failures;
}
