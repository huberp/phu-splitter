#pragma once

#include "MulticastBroadcasterBase.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace phu {
namespace network {

/**
 * SpectrumBroadcaster: UDP multicast broadcaster/receiver for sharing
 * frequency spectrum data between plugin instances in a DAW project.
 *
 * Architecture:
 * - Sender: called from a timer thread, compresses spectrum to dB-domain
 *   8-bit quantization and multicasts via UDP.
 * - Receiver: dedicated background thread receives packets, decompresses,
 *   and stores the latest spectrum per remote instance in a mutex-protected map.
 * - Reader: UI thread calls getReceivedSpectrums() to get a snapshot of all
 *   currently active remote instances' spectrums.
 *
 * Features:
 * - Non-blocking UDP multicast (IPv4)
 * - Mutex-protected map for reliable per-instance latest spectrum
 * - Automatic instance ID generation
 * - Configurable broadcast rate throttling
 * - dB-domain 8-bit quantization (80 dB dynamic range preserved)
 * - Automatic staleness pruning (instances silent > 3s are removed)
 *
 * Thread safety:
 * - broadcastSpectrum() is safe to call from any single thread (timer/UI)
 * - getReceivedSpectrums() is safe to call from any thread
 * - Receiver thread is managed internally
 */
class SpectrumBroadcaster : public MulticastBroadcasterBase {
  public:
    /** Multicast group address (administratively scoped, local org). */
    static constexpr const char* MULTICAST_GROUP = "239.255.42.1";

    /** UDP port for spectrum multicasts. */
    static constexpr int MULTICAST_PORT = 49421;

    /** Maximum spectrum bins to transmit per packet. */
    static constexpr int MAX_SPECTRUM_BINS = 512;

    /** dB range for quantization. Values below this floor are silent. */
    static constexpr float DB_FLOOR = -80.0f;

    /** dB ceiling for quantization. */
    static constexpr float DB_CEILING = 0.0f;

    /** Time in milliseconds after which a remote instance is considered stale. */
    static constexpr int64_t STALE_TIMEOUT_MS = 3000;

    // Spectrum packet structure (packed for network transmission)
    #pragma pack(push, 1)
    struct SpectrumPacket {
        uint32_t magic;        // Protocol magic number: 0x53504543 ("SPEC")
        uint32_t version;      // Protocol version: 2
        uint32_t instanceID;   // Unique instance identifier
        uint64_t timestamp;    // Timestamp in milliseconds
        uint16_t numBins;      // Number of spectrum bins (up to MAX_SPECTRUM_BINS)
        float sampleRate;      // Sample rate for frequency mapping
        uint8_t magnitudes[MAX_SPECTRUM_BINS]; // dB-quantized magnitudes (0-255)
    };
    #pragma pack(pop)

    /**
     * Received spectrum data from a remote plugin instance (unpacked for rendering).
     * Magnitudes are in linear scale, suitable for direct dB conversion in the renderer.
     */
    struct RemoteSpectrum {
        uint32_t instanceID = 0;
        int64_t timestamp = 0;
        float sampleRate = 0.0f;
        std::vector<float> magnitudes; ///< Linear-scale magnitudes (same domain as FFTProcessor output)
    };

    SpectrumBroadcaster();
    ~SpectrumBroadcaster() override = default;

    // Delete copy/move constructors
    SpectrumBroadcaster(const SpectrumBroadcaster&) = delete;
    SpectrumBroadcaster& operator=(const SpectrumBroadcaster&) = delete;

    /** Enable or disable broadcasting (default: enabled after initialize). */
    void setBroadcastEnabled(bool enabled) { broadcastEnabled.store(enabled); }

    /** Enable or disable receiving (default: enabled after initialize). */
    void setReceiveEnabled(bool enabled) { receiveEnabled.store(enabled); }

    /**
     * Set minimum interval between broadcasts (throttling).
     * @param intervalMs Minimum milliseconds between broadcasts (default: 33ms = ~30Hz)
     */
    void setBroadcastInterval(int intervalMs) { minBroadcastIntervalMs = intervalMs; }

    /**
     * Broadcast spectrum data to all instances on the multicast group.
     * Magnitudes are compressed to dB-domain 8-bit quantization before sending.
     *
     * @param magnitudes Magnitude spectrum array (linear scale, same as FFTProcessor output)
     * @param numBins Number of bins in the magnitude array
     * @param sampleRate Sample rate for frequency mapping on the receiver side
     * @return true if broadcast succeeded, false if throttled or error
     */
    bool broadcastSpectrum(const float* magnitudes, int numBins, float sampleRate);

    /**
     * Get latest received spectrum for each active remote instance.
     * Returns a snapshot — does not drain a queue. Stale entries (> STALE_TIMEOUT_MS)
     * are automatically pruned.
     *
     * @return Vector of the latest spectrum per remote instance
     */
    std::vector<RemoteSpectrum> getReceivedSpectrums();

    /** Get number of currently active remote instances. */
    int getNumRemoteInstances() const;

  protected:
    // MulticastBroadcasterBase overrides
    void receiverThreadRun() override;
    void onShutdown() override;

  private:
    // Spectrum-specific state
    std::atomic<bool> broadcastEnabled{true};
    std::atomic<bool> receiveEnabled{true};

    // Broadcast throttling
    int minBroadcastIntervalMs = 33; // ~30 Hz default
    int64_t lastBroadcastTime = 0;

    // Mutex-protected map: latest spectrum per remote instance ID.
    // The receiver thread writes, the UI thread reads via getReceivedSpectrums().
    mutable std::mutex receiveMutex;
    std::map<uint32_t, RemoteSpectrum> latestSpectrums;

    /**
     * Compress linear magnitudes to dB-domain 8-bit quantization.
     * Maps [DB_FLOOR, DB_CEILING] dB to [0, 255]. Values below DB_FLOOR become 0.
     */
    void compressSpectrum(const float* input, int inputBins, uint8_t* output, int outputBins);

    /**
     * Decompress 8-bit dB-quantized values back to linear magnitudes.
     */
    void decompressSpectrum(const uint8_t* input, int numBins, std::vector<float>& output);
};

} // namespace network
} // namespace phu
