# Instructions for GitHub Copilot

## Building This Project

This is a JUCE-based audio plugin project that requires specific dependencies on Linux.

### Before Building on Linux

1. **Install dependencies first**: Run `sudo bash scripts/install-linux-deps.sh`
2. **Dependencies are fetched by CMake**: JUCE (8.0.15) and phu-audio-lib are downloaded via FetchContent during the first configure (network access required); there are no submodules
3. **Use the Linux preset**: Build with `cmake --preset linux-x64-vst3-release && cmake --build --preset linux-x64-vst3-release-build`

### Build Timeouts

If builds timeout:
- Use fewer parallel jobs: `cmake --build --preset linux-x64-vst3-release-build -j2`
- Ensure all dependencies are installed before attempting to build
- Ensure the first configure can reach GitHub so JUCE and phu-audio-lib can be fetched

### Documentation

- Main README: [README.md](./README.md)
- Linux build guide: [docs/LINUX_BUILD.md](./docs/LINUX_BUILD.md)
- JUCE Linux dependencies: `JUCE/docs/Linux Dependencies.md` (available after the first configure)
