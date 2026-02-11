# Building on Linux

This guide covers building the phu-splitter VST3 plugin on Linux.

## Prerequisites

### Install Dependencies (Ubuntu/Debian)

Run the provided installation script:

```bash
sudo bash scripts/install-linux-deps.sh
```

Or install manually:

```bash
# Compiler and build tools
sudo apt-get install build-essential cmake ninja-build

# JUCE audio dependencies
sudo apt-get install libasound2-dev libjack-jackd2-dev

# JUCE graphics dependencies
sudo apt-get install libfreetype6-dev libfontconfig1-dev

# JUCE GUI dependencies
sudo apt-get install libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
```

**Note:** This plugin disables optional JUCE features (webkit, curl, ladspa) via CMake flags, so only the minimal dependencies listed above are required.

### Other Distributions

For Arch Linux, Fedora, or other distributions, install the equivalent packages for:
- X11 development libraries
- ALSA development libraries
- FreeType and FontConfig
- CMake and Ninja

## Building

### Using CMake Presets (Recommended)

```bash
cmake --preset linux-release
cmake --build --preset linux-build
```

### Manual Build

```bash
mkdir -p build/linux
cd build/linux
cmake ../.. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target phu-splitter_VST3
```

## Output

The built VST3 plugin will be located at:
```
build/linux-release/src/phu-splitter_artefacts/Release/VST3/PHU SPLITTER.vst3/
```

## Installation

Copy the VST3 to your system VST3 directory:

```bash
cp -r "build/linux-release/src/phu-splitter_artefacts/Release/VST3/PHU SPLITTER.vst3" \
    ~/.vst3/
```

## Troubleshooting

### Missing Dependencies

If CMake fails with dependency errors, ensure all JUCE dependencies are installed:

```bash
sudo bash scripts/install-linux-deps.sh
```

### Submodules Not Initialized

If you get JUCE-related errors:

```bash
git submodule update --init --recursive
```

### Build Timeouts

For resource-constrained systems, use fewer parallel jobs:

```bash
cmake --build --preset linux-build -j2
```
