# HeavyChop

A Flume-style motion + weight effect (VST3 for Windows, plus a standalone app).

```
input ─► [ HEAVY: 2x-oversampled saturation ─► 3-band OTT-style squash ─► low-shelf weight ] (parallel mix)
      ─► [ MOTION: tremolo / chop / pump / 16-step pattern gate, with slow→fast ramp ]
      ─► width ─► output ─► soft clip
```

## Controls

**Motion**
| Control | What it does |
|---|---|
| Shape | Sine / Triangle (tremolo), Chop (square gate), Pump (sidechain duck), Pattern (16-step gate) |
| Rate Start / Rate End | Note divisions, synced to host tempo (1/1 … 1/64, incl. triplets) |
| Ramp | Off, or 1/2 – 8 bars: sweeps the rate from Start to End (slow→fast) |
| Ramp Mode | **Loop** (restart every N bars, locked to the DAW grid), **Ping-Pong** (slow→fast→slow), **MIDI Trigger** (each MIDI note restarts the ramp and it holds at the end rate) |
| Curve | <50% speeds up early, >50% stays slow then rushes at the end (classic riser) |
| Depth | How deep the tremolo/chop goes |
| Gate Len | Pulse width for Chop/Pattern, recovery time for Pump |
| Smooth | Edge smoothing in ms: low = hard clicky chops, high = soft |
| Stereo | Right-channel phase offset (up to 180°, which gives auto-pan) |
| Steps + grid | Pattern length and on/off steps (used when Shape = Pattern) |

**Weight**
| Control | What it does |
|---|---|
| Drive | Asymmetric tanh saturation (adds even harmonics, so it sounds thicker) |
| Squash | Multiband upward + downward compression in the style of OTT (the Flume sound) |
| Weight | Low shelf at 90 Hz, up to +12 dB |
| Heavy Mix | Parallel blend of the heavy chain with the dry signal |
| Width | Mid/side width (0–200%) |
| Output / Soft Clip | Output level and a safety soft clipper |

There are 7 factory presets (dropdown at the top right, also exposed as host programs).

## Building on Windows

### Option A: no setup (GitHub Actions)
1. Push this folder to a GitHub repo.
2. The workflow `.github/workflows/build-windows.yml` runs automatically.
3. Open the run under **Actions** and download the **HeavyChop-VST3-Windows** artifact.
4. Unzip it and copy the `HeavyChop.vst3` folder to `C:\Program Files\Common Files\VST3\`.

### Option B: local build
Requirements: [Visual Studio 2022](https://visualstudio.microsoft.com/) (with "Desktop development with C++"), [CMake](https://cmake.org/) 3.22+ and Git.

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build\HeavyChop_artefacts\Release\VST3\HeavyChop.vst3`. Copy it to `C:\Program Files\Common Files\VST3\` and rescan in your DAW.
JUCE (8.0.9) is downloaded automatically the first time you configure.

Note: this builds VST3, which every modern DAW supports (FL Studio, Ableton, Bitwig, Reaper, Cubase). Building VST2 needs Steinberg's discontinued VST2 SDK.

## Using MIDI Trigger mode
Set Ramp Mode to **MIDI Trigger** and send MIDI notes to the plugin. In Ableton, route a MIDI track's output to the
HeavyChop device. In FL Studio, use a Patcher or set the MIDI input port. Each note restarts the slow→fast ramp.

## Code layout
- `Source/ModEngine.h`: tempo-synced LFO, ramp and pattern engine
- `Source/HeavyChain.h`: saturation, multiband squash and weight
- `Source/PluginProcessor.*`: parameters, presets and the signal chain
- `Source/PluginEditor.*`: UI
