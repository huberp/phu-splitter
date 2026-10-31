# Contributing to phu-splitter

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++20/JUCE code style
3. No memory allocation, system calls, or locks on the audio thread
4. Verify the project builds and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-splitter/issues).

---

## Building

Requires CMake 3.28+ and a C++20 compiler. JUCE (8.0.15) and phu-audio-lib are fetched by CMake during the first configure (network access required); there are no git submodules. They are checked out into `JUCE/` and `phu-audio-lib/` (git-ignored).

```bash
cmake --list-presets=all                                   # all presets
cmake --preset windows-x64-vst3-release                    # configure (or linux-x64-vst3-release, macos-arm64-release, ...)
cmake --build --preset windows-x64-vst3-release-build      # build
```

Build output goes to `build/<preset-name>/`. macOS presets build VST3 and AU; the formats are controlled by the `PHU_PLUGIN_FORMATS` cache variable.

Additional options and presets:
- `-DENABLE_STRICT_WARNINGS=ON` — strict compiler warnings for this project's code (not for fetched dependencies)
- `-DUSE_INTEL_MKL=OFF` — don't use Intel MKL for the FFT (default `ON`; falls back to JUCE's FFT when MKL isn't found). Set `MKLROOT` to point to an MKL install; on Windows the NuGet cache is detected automatically
- `--preset sanitizers` / `--preset sanitizers-build` — Address + Undefined Behavior sanitizers
- `--preset coverage` / `--preset coverage-build` — coverage instrumentation

---

## CI / Releases

GitHub Actions workflows live in `.github/workflows/`:

- `build.yml` — runs on pushes to `main`, pull requests and manual dispatch; builds Windows x64 (with pluginval), Linux x64/arm64 and macOS arm64/x64 (with pluginval).
- `build-windows-vst3.yml`, `build-linux.yml`, `build-macos.yml` — reusable per-platform workflows (`workflow_call`, inputs `arch`, `run-pluginval`, `upload-artifact`, `artifact-name`, `ref-name`) used by both `build.yml` and `release.yml`. They configure and build with the CMake presets, optionally validate with pluginval, and package a zip when `ref-name` is set.
- `generate-pdf.yml` — reusable workflow that converts `README.md` to `README.pdf` for the release zips.
- `release.yml` — triggered by pushing a tag matching `v[0-9]*`, or manually via **Run workflow** with a required `tag` input (the tag is created on the selected commit if it doesn't exist yet). It builds all platforms and publishes a GitHub release. See [docs/RELEASE.md](docs/RELEASE.md).

---

## Implementation Details

- **Linkwitz-Riley filters**: 8th order (48 dB/octave) crossovers implemented using cascaded biquad filters in Direct Form I
- **Phase coherent**: Allpass compensation ensures flat magnitude response when all bands are summed
- **Multi-output architecture**: 1 stereo input → 7 stereo output buses
- **Real-time safe**: Lock-free logging system for editor updates from audio thread
- **Note-to-frequency conversion**: Header-only `NoteToFreq` utility in `lib/`

## Project Structure

| Path | Description |
|------|-------------|
| `src/LinkwitzRileyFilter.h` | Core DSP — biquad filters, LR crossovers, MultiBandN |
| `src/PluginProcessor.cpp/.h` | Audio processor with APVTS parameter management |
| `src/PluginEditor.cpp/.h` | Plugin editor UI layout |
| `src/CrossoverFrequencyBar.cpp/.h` | Draggable frequency bar with text input |
| `src/PresetManager.cpp/.h` | Preset storage engine |
| `src/PresetStrip.cpp/.h` | Inline preset navigation UI |
| `lib/NoteToFreq.h` | Musical note name → frequency converter |
| `lib/` | Event system library (see `lib/README.md`) |
| `cmake/ConfigureIntelMKL.cmake` | Optional Intel MKL FFT acceleration |
