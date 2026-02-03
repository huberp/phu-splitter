# phu-splitter

JUCE-based **VST3 audio effect** that splits a stereo input into **7 frequency bands** using Linkwitz-Riley crossover filters.

Each band is output as a separate stereo channel, enabling multiband processing workflows in a DAW.

## Build (CMake + Visual Studio)

This repo is set up for CMake presets.

- Configure: `cmake --preset vs2026-x64`
- Build Debug: `cmake --build --preset debug`
- Build Release: `cmake --build --preset release`

The target built by the presets is `phu-splitter_VST3`.

## Audio Processing

The plugin uses **Linkwitz-Riley crossover filters** to split the incoming stereo audio into 7 frequency bands:

1. **Sub bass** (< 80 Hz)
2. **Bass** (80-250 Hz)
3. **Low-mid** (250-500 Hz)
4. **Mid** (500-2000 Hz)
5. **Upper-mid** (2000-6000 Hz)
6. **Presence** (6000-12000 Hz)
7. **Brilliance** (> 12000 Hz)

Each band is output to a separate stereo bus, allowing independent processing in your DAW.

### Default Crossover Frequencies

- 80 Hz (Sub bass / Bass split)
- 250 Hz (Bass / Low-mid split)
- 500 Hz (Low-mid / Mid split)
- 2000 Hz (Mid / Upper-mid split)
- 6000 Hz (Upper-mid / Presence split)
- 12000 Hz (Presence / Brilliance split)

The crossover uses 48 dB/octave slopes for steep transitions and excellent frequency separation.

## Implementation details

- **Linkwitz-Riley filters**: 8th order (48 dB/octave) crossovers implemented using cascaded biquad filters
- **Phase coherent**: When all bands are summed, the original signal is reconstructed with flat magnitude response
- **Multi-output architecture**: 1 stereo input → 7 stereo output buses
- **Real-time safe**: Lock-free logging system for editor updates from audio thread

## Where to look

- Linkwitz-Riley filter implementation: `src/LinkwitzRileyFilter.h`
- Plugin processor: `src/PluginProcessor.cpp` and `src/PluginProcessor.h`
- Event system used by the plugin: `lib/README.md`