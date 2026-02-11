# Instructions for GitHub Copilot

## Building This Project

This is a JUCE-based audio plugin project that requires specific dependencies on Linux.

### Before Building on Linux

1. **Install dependencies first**: Run `sudo bash scripts/install-linux-deps.sh`
2. **Initialize submodules**: Ensure JUCE submodule is initialized with `git submodule update --init --recursive`
3. **Use the Linux preset**: Build with `cmake --preset linux-release && cmake --build --preset linux-build`

### Build Timeouts

If builds timeout:
- Use fewer parallel jobs: `cmake --build --preset linux-build -j2`
- Ensure all dependencies are installed before attempting to build
- The JUCE submodule must be fully initialized

### Documentation

- Main README: [README.md](./README.md)
- Linux build guide: [docs/LINUX_BUILD.md](./docs/LINUX_BUILD.md)
- JUCE Linux dependencies: [JUCE/docs/Linux Dependencies.md](./JUCE/docs/Linux%20Dependencies.md)
