## Render smoothed spectrum line in crossover UI

### Depends on
Issue 2: Implement configurable FFT analysis with windowing and damping on UI thread

### Goal
Add a visible, smoothed spectrum curve to the existing crossover UI, drawn inside the crossover display area using the damped FFT magnitudes from Issue 2.

### Tasks
- [ ] Identify or create the UI component responsible for the crossover display (the "crossover window").
  - [ ] Expose a way for this component to access:
    - [ ] `smoothedMagnitudes` from the FFT analysis engine
    - [ ] Current sample rate
    - [ ] Current FFT size (to derive bin→frequency mapping)
- [ ] Define visual mapping:
  - [ ] x-axis: map FFT bins to frequency, then to pixel x:
    - [ ] Implement at least one mapping:
      - Log frequency from ~20 Hz to Nyquist
  - [ ] y-axis: map magnitudes to pixel y:
    - [ ] Decide dB (or linear) range, e.g. −90 dB to 0 dB
- [ ] Implement drawing of a smoothed line:
  - [ ] Build a continuous path from `smoothedMagnitudes` across the crossover area
  - [ ] Consider additional internal smoothing in screen space (e.g. skip some bins for performance, or average neighboring bins)
  - [ ] Draw this path on top of or underneath existing crossover graphics (choose z-order)
- [ ] Integrate with the FFT update loop:
  - [ ] Ensure the crossover component repaints whenever new FFT data is available (reuse the timer from Issue 2 or attach a listener)
- [ ] Align the spectrum with crossover bands:
  - [ ] Use current band crossover frequencies to:
    - [ ] Optionally draw vertical markers/gridlines
    - [ ] Confirm that the spectrum curve visually aligns within the same coordinate system as the crossover bands
- [ ] Add basic styling:
  - [ ] Color and thickness for the spectrum line
  - [ ] Optional fill/alpha under curve (if desired and not too busy)
- [ ] Verify performance:
  - [ ] Confirm that repaint + spectrum drawing at target FPS does not noticeably impact UI responsiveness
  - [ ] Confirm that no UI-thread work leaks into the audio thread

### Acceptance criteria
- The crossover UI shows a continuously updating, smoothed spectrum line overlaid (or embedded) in the crossover window.
- The spectrum curve reacts to input audio and follows the band range logically (e.g. more energy in certain bands where expected).
- FFT size changes (from Issue 2) are reflected visually in the spectrum resolution.
- UI remains responsive; no impact on audio stability.