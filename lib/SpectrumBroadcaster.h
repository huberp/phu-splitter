#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <memory>
#include <thread>
#include <vector>

// Forward declare socket type to avoid including Windows headers in header file
#ifdef _WIN32
    using socket_t = unsigned long long; // SOCKET type on Windows
#else
    using socket_t = int;
#endif

/**
 * SpectrumBroadcaster: UDP multicast broadcaster/receiver for sharing
 * frequency spectrum data between plugin instances in a DAW project.
 *
 * Features:
 * - Non-blocking UDP multicast (IPv4)
 * - Lock-free FIFO for thread-safe communication
 * - Automatic instance ID generation
 * - Configurable broadcast rate throttling
 * - Compressed spectrum data (8-bit quantization)
 *
 * Usage:
 *   SpectrumBroadcaster broadcaster;
 *   broadcaster.initialize();
 *   broadcaster.broadcastSpectrum(magnitudes, numBins, sampleRate);
 *   auto remoteSpectrums = broadcaster.getReceivedSpectrums();
 */
class SpectrumBroadcaster {
  public:
    // Multicast group and port
    static constexpr const char* MULTICAST_GROUP = "239.255.42.1";
    static constexpr int MULTICAST_PORT = 49421;

    // Maximum spectrum bins to transmit (compression)
    static constexpr int MAX_SPECTRUM_BINS = 512;

    // Spectrum packet structure (packed for network transmission)
    #pragma pack(push, 1)
    struct SpectrumPacket {
        uint32_t magic;        // Protocol magic number: 0x53504543 ("SPEC")
        uint32_t version;      // Protocol version: 1
        uint32_t instanceID;   // Unique instance identifier
        uint64_t timestamp;    // Timestamp in milliseconds
        uint16_t numBins;      // Number of spectrum bins (up to MAX_SPECTRUM_BINS)
        float sampleRate;      // Sample rate for frequency mapping
        uint8_t magnitudes[MAX_SPECTRUM_BINS]; // Quantized magnitudes (0-255)
    };
    #pragma pack(pop)

    // Received spectrum data (unpacked for rendering)
    struct RemoteSpectrum {
        uint32_t instanceID;
        uint64_t timestamp;
        float sampleRate;
        std::vector<float> magnitudes; // Dequantized to float
    };

    SpectrumBroadcaster();
    ~SpectrumBroadcaster();

    // Delete copy/move constructors
    SpectrumBroadcaster(const SpectrumBroadcaster&) = delete;
    SpectrumBroadcaster& operator=(const SpectrumBroadcaster&) = delete;

    /**
     * Initialize networking (sockets, multicast group)
     * @return true if successful, false on error
     */
    bool initialize();

    /**
     * Shutdown networking and clean up resources
     */
    void shutdown();

    /**
     * Check if broadcaster is initialized and running
     */
    bool isRunning() const { return running.load(); }

    /**
     * Get this instance's unique ID
     */
    uint32_t getInstanceID() const { return instanceID; }

    /**
     * Enable or disable broadcasting (default: enabled)
     */
    void setBroadcastEnabled(bool enabled) { broadcastEnabled.store(enabled); }

    /**
     * Enable or disable receiving (default: enabled)
     */
    void setReceiveEnabled(bool enabled) { receiveEnabled.store(enabled); }

    /**
     * Set minimum interval between broadcasts (throttling)
     * @param intervalMs Minimum milliseconds between broadcasts (default: 33ms = ~30Hz)
     */
    void setBroadcastInterval(int intervalMs) { minBroadcastIntervalMs = intervalMs; }

    /**
     * Broadcast spectrum data to all instances
     * @param magnitudes Magnitude spectrum array (linear scale)
     * @param numBins Number of bins in the magnitude array
     * @param sampleRate Sample rate for frequency mapping
     * @return true if broadcast succeeded, false if throttled or error
     */
    bool broadcastSpectrum(const float* magnitudes, int numBins, float sampleRate);

    /**
     * Get all received spectrums since last call (consumes queue)
     * Called from UI thread to retrieve remote instance data
     * @return Vector of received spectrum data
     */
    std::vector<RemoteSpectrum> getReceivedSpectrums();

    /**
     * Get number of remote spectrums currently in receive queue
     */
    int getNumReceivedSpectrums() const { return receiveFifo.getNumReady(); }

  private:
    // Network state
    socket_t sendSocket;
    socket_t recvSocket;
    void* multicastAddr; // sockaddr_in* (opaque pointer to avoid including socket headers)
    bool networkInitialized;

    // Instance identification
    uint32_t instanceID;

    // Thread management
    std::atomic<bool> running{false};
    std::atomic<bool> broadcastEnabled{true};
    std::atomic<bool> receiveEnabled{true};
    std::unique_ptr<std::thread> receiverThread;

    // Broadcast throttling
    int minBroadcastIntervalMs = 33; // ~30 Hz default
    int64_t lastBroadcastTime = 0;

    // Lock-free FIFO for received spectrums (receiver thread -> UI thread)
    static constexpr int FIFO_SIZE = 32;
    juce::AbstractFifo receiveFifo{FIFO_SIZE};
    std::array<RemoteSpectrum, FIFO_SIZE> spectrumBuffer;

    // Receiver thread function
    void receiverThreadRun();

    // Helper functions
    bool initializeSockets();
    void cleanupSockets();
    uint32_t generateInstanceID();
    int64_t getCurrentTimeMs() const;

    // Spectrum compression/decompression
    void compressSpectrum(const float* input, int inputBins, uint8_t* output, int outputBins);
    void decompressSpectrum(const uint8_t* input, int numBins, std::vector<float>& output);

#ifdef _WIN32
    // Windows-specific: WSA initialization
    static bool wsaInitialized;
    static int wsaRefCount;
    static std::mutex wsaMutex;
    bool initializeWSA();
    void cleanupWSA();
#endif
};
