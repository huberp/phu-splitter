# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0-alpha] - Unreleased

### Added
- Initial alpha release
- JUCE-based VST3 audio effect plugin
- 7-band frequency splitter using Linkwitz-Riley crossover filters
- Multi-output architecture: 1 stereo input → 7 stereo output buses
- Default crossover frequencies at 80 Hz, 250 Hz, 500 Hz, 2000 Hz, 6000 Hz, and 12000 Hz
- 8th order (48 dB/octave) Linkwitz-Riley filters for steep transitions
- Phase coherent signal reconstruction when all bands are summed
- Real-time safe lock-free logging system for editor updates
- Event system library for plugin communication
