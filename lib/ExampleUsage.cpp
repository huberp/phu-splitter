/**
 * Example Usage - Demonstrates the C++ EventSource pattern
 *
 * This file shows how to use the EventSource system to match
 * the Lua pattern from your ProtoplugScripts.
 */

#include "EventSource.h"
#include "SyncGlobals.h"
#include <iostream>

/**
 * Example 1: Simple listener for GLOBALS events
 * Mirrors: GLOBALS:addEventListener(function(inEvent) ... end)
 */
class SimpleGlobalsListener : public GlobalsEventListener
{
  public:
    void onBPMChanged(const BPMEvent& event) override
    {
        std::cout << "BPM changed from " << event.oldValues.bpm << " to " << event.newValues.bpm
                  << std::endl;
        std::cout << "  New samples per beat: " << event.newValues.samplesPerBeat << std::endl;
    }

    void onIsPlayingChanged(const IsPlayingEvent& event) override
    {
        std::cout << "Playing state changed: " << (event.newValue ? "PLAYING" : "STOPPED")
                  << std::endl;
    }
};

/**
 * Example 2: BUFFERS object that listens to GLOBALS
 * Mirrors the Lua pattern:
 *   GLOBALS:addEventListener(function(inEvent) BUFFERS:listenToGlobalsChange(inEvent) end)
 */
class BuffersManager : public GlobalsEventListener, public BufferEventSource
{
  private:
    int numBeats = 1;
    int globalSize = 0;
    double samplesPerBeat = 0.0;

  public:
    // Listen to GLOBALS BPM changes and update internal state
    void onBPMChanged(const BPMEvent& event) override
    {
        // Update our internal timing
        samplesPerBeat = event.newValues.samplesPerBeat;
        globalSize = static_cast<int>(samplesPerBeat * numBeats);

        std::cout << "BUFFERS: Reacting to BPM change, new globalSize = " << globalSize
                  << std::endl;

        // Fire our own event to downstream listeners
        BuffersChangedEvent bufferEvent;
        bufferEvent.source = this;
        bufferEvent.context = event.context;
        bufferEvent.numBeats = numBeats;
        bufferEvent.globalSize = globalSize;
        bufferEvent.samplesPerBeat = samplesPerBeat;

        fireBuffersChanged(bufferEvent);
    }

    void onSampleRateChanged(const SampleRateEvent& event) override
    {
        std::cout << "BUFFERS: Sample rate changed to " << event.newRate << std::endl;
    }

    void setNumBeats(int beats)
    {
        numBeats = beats;
    }
};

/**
 * Example 3: CLIENT_PATHS that listens to BUFFERS
 * Mirrors the Lua pattern:
 *   BUFFERS:addEventListener(function(inEvent) CLIENT_PATHS:listenToBufferChanges(inEvent) end)
 */
class ClientPathsManager : public BufferEventListener
{
  public:
    void onBuffersChanged(const BuffersChangedEvent& event) override
    {
        std::cout << "CLIENT_PATHS: Buffers changed, globalSize = " << event.globalSize
                  << std::endl;
        // Would update client path buckets here
    }
};

/**
 * Example 4: RMS calculator that also listens to BUFFERS
 * Mirrors the Lua pattern:
 *   BUFFERS:addEventListener(function(inEvent) RMS:listenToBufferChanges(inEvent) end)
 */
class RMSCalculator : public BufferEventListener
{
  public:
    void onBuffersChanged(const BuffersChangedEvent& event) override
    {
        std::cout << "RMS: Reconfiguring buckets for " << event.numBeats << " beats" << std::endl;
        // Would reconfigure RMS buckets here
    }
};

/**
 * Main example demonstrating the event flow
 */
int main()
{
    std::cout << "=== C++ EventSource Example ===" << std::endl;
    std::cout << std::endl;

    // Get the singleton GLOBALS instance
    auto& globals = SyncGlobals::getInstance();

    // Create a simple listener
    SimpleGlobalsListener simpleListener;
    globals.addEventListener(&simpleListener);

    // Create BUFFERS and connect it to GLOBALS
    BuffersManager buffers;
    globals.addEventListener(&buffers);

    // Create downstream listeners for BUFFERS
    ClientPathsManager clientPaths;
    RMSCalculator rms;
    buffers.addEventListener(&clientPaths);
    buffers.addEventListener(&rms);

    std::cout << "Event system initialized." << std::endl;
    std::cout << "GLOBALS has " << globals.getListenerCount() << " listeners" << std::endl;
    std::cout << "BUFFERS has " << buffers.getListenerCount() << " listeners" << std::endl;
    std::cout << std::endl;

    // Simulate a sample rate change
    std::cout << "--- Simulating sample rate change ---" << std::endl;
    globals.updateSampleRate(48000);
    std::cout << std::endl;

    // Simulate a BPM change
    std::cout << "--- Simulating BPM change ---" << std::endl;
    BPMEvent bpmEvent;
    bpmEvent.source = &globals;
    bpmEvent.oldValues = {120.0, 500.0, 24000.0};
    bpmEvent.newValues = {140.0, 428.57, 20571.4};
    globals.fireBPMChanged(bpmEvent);
    std::cout << std::endl;

    // Simulate playback start
    std::cout << "--- Simulating playback start ---" << std::endl;
    IsPlayingEvent playEvent;
    playEvent.source = &globals;
    playEvent.oldValue = false;
    playEvent.newValue = true;
    globals.fireIsPlayingChanged(playEvent);
    std::cout << std::endl;

    // Clean up - remove listeners
    std::cout << "--- Cleaning up ---" << std::endl;
    globals.removeEventListener(&simpleListener);
    globals.removeEventListener(&buffers);
    buffers.removeEventListener(&clientPaths);
    buffers.removeEventListener(&rms);

    std::cout << "Listeners removed." << std::endl;
    std::cout << "GLOBALS has " << globals.getListenerCount() << " listeners" << std::endl;
    std::cout << "BUFFERS has " << buffers.getListenerCount() << " listeners" << std::endl;

    return 0;
}
