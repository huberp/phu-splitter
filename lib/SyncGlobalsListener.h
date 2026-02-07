#pragma once

#include "Event.h"

/**
 * BPM Changed Event
 * Fired when DAW tempo changes
 */
struct BPMEvent : public Event {
    struct Values {
        double bpm = 0.0;
        double msecPerBeat = 0.0;
        double samplesPerBeat = 0.0;
    };

    Values oldValues;
    Values newValues;
};

/**
 * Playing State Changed Event
 * Fired when DAW starts/stops playback
 */
struct IsPlayingEvent : public Event {
    bool oldValue = false;
    bool newValue = false;
};

/**
 * Sample Rate Changed Event
 * Fired when audio sample rate changes
 */
struct SampleRateEvent : public Event {
    double oldRate = 0.0;
    double newRate = 0.0;
};

/**
 * Listener interface for GLOBALS events (BPM, IsPlaying, SampleRate)
 *
 * Objects that need to respond to DAW global state changes should inherit
 * from this interface and implement the relevant callbacks.
 *
 * Mirrors the Lua pattern:
 *   GLOBALS:addEventListener(function(inEvent) ... end)
 */
class GlobalsEventListener {
  public:
    virtual ~GlobalsEventListener() = default;

    /**
     * Called when BPM changes
     * @param event Contains old and new BPM values plus derived timing info
     */
    virtual void onBPMChanged(const BPMEvent& event) {
        // Default empty implementation - override if needed
        (void)event; // Suppress unused warning
    }

    /**
     * Called when playback starts or stops
     * @param event Contains old and new playing state
     */
    virtual void onIsPlayingChanged(const IsPlayingEvent& event) {
        // Default empty implementation - override if needed
        (void)event;
    }

    /**
     * Called when sample rate changes
     * @param event Contains old and new sample rate
     */
    virtual void onSampleRateChanged(const SampleRateEvent& event) {
        // Default empty implementation - override if needed
        (void)event;
    }
};