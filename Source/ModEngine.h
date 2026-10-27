#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

// Tempo-synced modulation engine: tremolo / chop / pump / step-pattern gate,
// with a "slow -> fast" ramp that sweeps the rate between two note divisions.
//
// Rates are in cycles per beat. Phase is tracked as an unwrapped "cycles" count so
// the integer part gives the pattern step and the fractional part the LFO phase.
class ModEngine
{
public:
    enum Shape { Sine, Triangle, Square, Pump, Pattern };
    enum RampMode { Loop, PingPong, MidiTrigger };

    struct Settings
    {
        double rateStart = 1.0;   // cycles per beat at ramp start
        double rateEnd   = 1.0;   // cycles per beat at ramp end
        double rampBeats = 0.0;   // 0 = no ramp, just rateStart
        int    mode      = Loop;
        double curveExp  = 1.0;   // >1 = stays slow then rushes, <1 = speeds up early
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        rampPos = 0.0;
        cycles = 0.0;
    }

    void setSettings (const Settings& s) { settings = s; }

    void retrigger() { reset(); }

    // Call once per block. Locks phase to the host timeline when playing (Loop / PingPong).
    void syncToHost (bool isPlaying, double ppq)
    {
        if (! isPlaying || settings.mode == MidiTrigger)
            return;

        if (settings.rampBeats <= 0.0)
        {
            cycles = ppq * settings.rateStart;
            rampPos = 0.0;
            return;
        }

        const double len = cycleLength();
        double target = std::fmod (ppq, len);
        if (target < 0.0)
            target += len;

        rampPos = target;
        cycles = integrateCycles (target);
    }

    // Advance one sample; beatsPerSample = bpm / 60 / sampleRate
    void advance (double beatsPerSample)
    {
        cycles += rateAt (rampPos) * beatsPerSample;
        rampPos += beatsPerSample;

        if (settings.rampBeats <= 0.0)
            return;

        if (settings.mode == MidiTrigger)
        {
            rampPos = std::min (rampPos, settings.rampBeats); // hold the end rate
        }
        else
        {
            const double len = cycleLength();
            if (rampPos >= len)
            {
                rampPos = std::fmod (rampPos, len);
                cycles = rateAt (0.0) * rampPos; // restart on the grid
            }
        }
    }

    double getCycles() const { return cycles; }
    double getCurrentRate() const { return rateAt (rampPos); }

    // LFO value in [0, 1] (1 = full volume) for a given unwrapped phase
    static float shapeValue (int shape, double c, float duty,
                             const std::array<bool, 16>& steps, int patternLength)
    {
        const double fl = std::floor (c);
        const float frac = (float) (c - fl);

        switch (shape)
        {
            case Sine:     return 0.5f + 0.5f * std::cos (6.283185307f * frac);
            case Triangle: return std::abs (1.0f - 2.0f * frac);
            case Square:   return frac < duty ? 1.0f : 0.0f;
            case Pump:
            {
                // sidechain-style duck on every hit, recovering over "duty" of the cycle
                float x = std::min (1.0f, frac / std::max (0.01f, duty));
                return x * x * (3.0f - 2.0f * x);
            }
            case Pattern:
            default:
            {
                const int len = std::max (1, patternLength);
                int step = (int) (((int64_t) fl) % len);
                if (step < 0)
                    step += len;
                return (steps[(size_t) step] && frac < duty) ? 1.0f : 0.0f;
            }
        }
    }

    static int stepIndex (double c, int patternLength)
    {
        const int len = std::max (1, patternLength);
        int step = (int) (((int64_t) std::floor (c)) % len);
        return step < 0 ? step + len : step;
    }

private:
    double cycleLength() const
    {
        return settings.mode == PingPong ? 2.0 * settings.rampBeats : settings.rampBeats;
    }

    double rateAt (double pos) const
    {
        if (settings.rampBeats <= 0.0)
            return settings.rateStart;

        double u = pos / settings.rampBeats;
        if (settings.mode == PingPong && u > 1.0)
            u = 2.0 - u;
        u = std::clamp (u, 0.0, 1.0);

        // interpolate in log space so the speed-up sounds even (musical)
        const double shaped = std::pow (u, settings.curveExp);
        return settings.rateStart * std::pow (settings.rateEnd / settings.rateStart, shaped);
    }

    // Phase accumulated from the start of the ramp cycle up to pos (midpoint rule)
    double integrateCycles (double pos) const
    {
        constexpr int n = 512;
        const double dx = pos / n;
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += rateAt ((i + 0.5) * dx);
        return sum * dx;
    }

    Settings settings;
    double sampleRate = 44100.0;
    double rampPos = 0.0;
    double cycles = 0.0;
};
