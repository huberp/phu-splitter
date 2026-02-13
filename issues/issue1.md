## Introduce audio→UI sample transport (stereo + summed) via ring buffers

### Goal
Create the basic infrastructure to move audio from processBlock (audio thread) into the UI thread for analysis/visualization, keeping incoming stereo samples and a mono sum separately.

### Tasks
- [ ] Add configuration constants for spectrum analysis:
  - [ ] spectrumFftSize (initial default, e.g. 2048)
  - [ ] spectrumRingSize (e.g. 4× or 8× spectrumFftSize)
- [ ] In the audio processor class, add ring buffers and indices for:
  - [ ] Left input: ringL[spectrumRingSize]
  - [ ] Right input: ringR[spectrumRingSize]
  - [ ] Mono sum used for visualization: ringSum[spectrumRingSize]
  - [ ] Atomic write index: std::atomic<int> spectrumWritePos
- [ ] In prepareToPlay, initialize:
  - [ ] Ring buffers (zero fill)
  - [ ] spectrumWritePos to 0
- [ ] In processBlock:
  - [ ] Read L/R channel pointers from the incoming buffer
  - [ ] For each block:
    - [ ] Copy L into ringL using memcpy with wrap (up to 2 memcpys)
    - [ ] Copy R into ringR using memcpy with wrap
    - [ ] Compute mono sum 0.5 * (L + R) in a tight loop and write into ringSum with wrap
    - [ ] Update spectrumWritePos atomically after each block
- [ ] Expose a lightweight, thread-safe read API for the UI side, for example:
  - [ ] A small SpectrumSampleProvider or similar that:
    - [ ] Holds references to ringL, ringR, ringSum, and spectrumWritePos
    - [ ] Provides functions like:
      - copyLatestMonoFrame(float* dest, int numSamples)
      - copyLatestStereoFrame(float* destL, float* destR, int numSamples)
- [ ] Ensure no locks and no allocations occur in processBlock
- [ ] Add minimal tests / logging hooks (if feasible) to confirm:
  - [ ] Data is being written to the ring buffers
  - [ ] spectrumWritePos wraps correctly

### Acceptance criteria
- Ring buffers for L, R, and mono sum exist and are filled from processBlock without allocation/locks.
- UI thread can request the latest N samples (for some N <= spectrumRingSize) as mono or stereo via a dedicated provider.
- The rest of the plugin behavior is unchanged (no visual yet).