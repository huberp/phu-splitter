#pragma once

#include "EventSource.h"
#include "SyncGlobalsListener.h"
#include <cstddef>
#include <juce_audio_processors/juce_audio_processors.h>
/**
 * EventSource for GLOBALS events
 *
 * Manages listeners for BPM, IsPlaying, and SampleRate events.
 * Mirrors the Lua EventSource mixed into GLOBALS table.
 *
 * Usage:
 *   GlobalsEventSource globals;
 *   globals.addEventListener(&myListener);
 *   globals.fireBPMChanged(bpmEvent);
 */
class GlobalsEventSource : public EventSource<GlobalsEventListener>
{
  public:
    /**
     * Fire a BPM changed event to all listeners
     * @param event The BPM event to fire
     */
    void fireBPMChanged(const BPMEvent& event)
    {
        // Iterate by index to handle potential modifications during iteration
        for (size_t i = 0; i < listeners.size(); ++i)
        {
            listeners[i]->onBPMChanged(event);
        }
    }

    /**
     * Fire an IsPlaying changed event to all listeners
     * @param event The IsPlaying event to fire
     */
    void fireIsPlayingChanged(const IsPlayingEvent& event)
    {
        for (size_t i = 0; i < listeners.size(); ++i)
        {
            listeners[i]->onIsPlayingChanged(event);
        }
    }

    /**
     * Fire a SampleRate changed event to all listeners
     * @param event The SampleRate event to fire
     */
    void fireSampleRateChanged(const SampleRateEvent& event)
    {
        for (size_t i = 0; i < listeners.size(); ++i)
        {
            listeners[i]->onSampleRateChanged(event);
        }
    }
};
/**
 * SyncGlobals - Singleton that tracks DAW global state
 *
 * This is a C++ translation of the Lua SyncGlobals module.
 * Tracks BPM, sample rate, and playing state, firing events when they change.
 *
 * Usage:
 *   auto& globals = SyncGlobals::getInstance();
 *   globals.addEventListener(&myListener);
 *   globals.updateDAWGlobals(samples, numSamples, midiBuffer, position);
 */
class SyncGlobals : public GlobalsEventSource
{
  private:
    // PPQ (Pulses Per Quarter) base values
    struct PPQBaseValue
    {
        double msec = 60000.0; // milliseconds per minute
        double noteNum = 1.0;
        double noteDenom = 4.0;
        double ratio = 0.25; // noteNum / noteDenom
    } ppqBase;

    // Global state
    long runs = 0;              // Number of processBlock calls
    long long samplesCount = 0; // Total samples processed
    double sampleRate = -1.0;
    double sampleRateByMsec = -1.0; // Samples per millisecond
    bool isPlaying = false;
    double bpm = 0.0;
    double msecPerBeat = 0.0;    // Based on whole note
    double samplesPerBeat = 0.0; // Based on whole note

  public:
    // Default constructor
    SyncGlobals() = default;

    // Delete copy constructor and assignment
    SyncGlobals(const SyncGlobals&) = delete;
    SyncGlobals& operator=(const SyncGlobals&) = delete;

    /**
     * Mark end of processing run
     * @param numSamples Number of samples processed in this block
     */
    void finishRun(int numSamples)
    {
        runs++;
        samplesCount += numSamples;
    }

    /**
     * Get current run count, i.e. number of processBlock calls
     * Method finishRun increments this value
     */
    long getCurrentRun() const
    {
        return runs;
    }
    /**
     * Get total samples processed
     */
    long long getCurrentSampleCount() const
    {
        return samplesCount;
    }

    /**
     * Get current BPM
     */
    double getBPM() const
    {
        return bpm;
    }

    /**
     * Get current sample rate
     */
    double getSampleRate() const
    {
        return sampleRate;
    }

    /**
     * Check if DAW is playing
     */
    bool isDawPlaying() const
    {
        return isPlaying;
    }

    /**
     * Update sample rate (fires event if changed)
     * @param newSampleRate New sample rate in Hz
     */
    void updateSampleRate(double newSampleRate)
    {
        if (newSampleRate != sampleRate)
        {
            double oldSampleRate = sampleRate;
            sampleRate = newSampleRate;
            sampleRateByMsec = newSampleRate / 1000.0;

            // Recalculate samples per beat if we have a BPM
            if (bpm > 0.0)
            {
                samplesPerBeat = msecPerBeat * sampleRateByMsec;
            }

            // Fire event
            SampleRateEvent event;
            event.source = this;
            event.oldRate = oldSampleRate;
            event.newRate = newSampleRate;

            fireSampleRateChanged(event);
        }
    }

    /**
     * Update DAW globals at the beginning of a new frame
     * This mirrors the Lua updateDAWGlobals function
     *
     * @param buffer Audio buffer for this frame
     * @param midiBuffer MIDI buffer for this frame
     * @param positionInfo DAW position info
     * @return Context object for this frame
     */
    Event::Context
    updateDAWGlobals(const juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiBuffer,
                     const juce::Optional<juce::AudioPlayHead::PositionInfo>& positionInfo)
    {
        // Create context for this frame
        Event::Context ctx;
        ctx.buffer = &buffer;
        ctx.numberOfSamplesInFrame = buffer.getNumSamples();
        ctx.midiBuffer = &midiBuffer;
        ctx.positionInfo = &positionInfo;
        ctx.epoch = runs;

        // Extract playing state from position info

        if (positionInfo.hasValue())
        {
            // Extract BPM if available
            if (auto bpmValue = positionInfo->getBpm())
            {
                double newBPM = *bpmValue;
                if (newBPM != bpm && newBPM > 0.0)
                {
                    BPMEvent event;
                    event.source = this;
                    event.context = ctx;
                    event.oldValues = {bpm, msecPerBeat, samplesPerBeat};

                    // Update values
                    bpm = newBPM;
                    msecPerBeat = ppqBase.msec / newBPM;
                    samplesPerBeat = msecPerBeat * sampleRateByMsec;

                    event.newValues = {bpm, msecPerBeat, samplesPerBeat};
                    fireBPMChanged(event);
                }
            }

            bool newIsPlaying = positionInfo->getIsPlaying();
            // Check playing state change
            if (newIsPlaying != isPlaying)
            {
                IsPlayingEvent event;
                event.source = this;
                event.context = ctx;
                event.oldValue = isPlaying;
                event.newValue = newIsPlaying;
                isPlaying = newIsPlaying;
                fireIsPlayingChanged(event);
            }
        }
        return ctx;
    }
};
