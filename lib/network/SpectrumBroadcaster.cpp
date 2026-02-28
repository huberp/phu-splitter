#include "SpectrumBroadcaster.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace phu {
namespace network {

// Include socket headers only in implementation file (needed for sendto/recvfrom)
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #define INVALID_SOCKET_VALUE -1
#endif

// Protocol magic number: "SPEC" in ASCII
static constexpr uint32_t PROTOCOL_MAGIC = 0x53504543;
static constexpr uint32_t PROTOCOL_VERSION = 2;

// ============================================================================
// Construction
// ============================================================================

SpectrumBroadcaster::SpectrumBroadcaster()
    : MulticastBroadcasterBase(MULTICAST_GROUP, MULTICAST_PORT) {}

// ============================================================================
// Shutdown hook
// ============================================================================

void SpectrumBroadcaster::onShutdown() {
    std::lock_guard<std::mutex> lock(receiveMutex);
    latestSpectrums.clear();
}

// ============================================================================
// Broadcasting
// ============================================================================

bool SpectrumBroadcaster::broadcastSpectrum(const float* magnitudes, int numBins,
                                            float sampleRate) {
    if (!networkInitialized || !broadcastEnabled.load() || sendSocket == INVALID_SOCKET_VALUE) {
        return false;
    }

    // Throttle broadcasts
    int64_t now = getCurrentTimeMs();
    if (now - lastBroadcastTime < minBroadcastIntervalMs) {
        return false; // Throttled
    }
    lastBroadcastTime = now;

    // Create packet
    SpectrumPacket packet;
    std::memset(&packet, 0, sizeof(packet));
    packet.magic = PROTOCOL_MAGIC;
    packet.version = PROTOCOL_VERSION;
    packet.instanceID = instanceID;
    packet.timestamp = static_cast<uint64_t>(now);
    packet.sampleRate = sampleRate;

    // Compress spectrum (downsample and dB-quantize)
    int outputBins = (std::min)(numBins, MAX_SPECTRUM_BINS);
    packet.numBins = static_cast<uint16_t>(outputBins);
    compressSpectrum(magnitudes, numBins, packet.magnitudes, outputBins);

    // Send packet
    auto* addr = static_cast<sockaddr_in*>(multicastAddr);
    int bytesSent =
        sendto(sendSocket, reinterpret_cast<const char*>(&packet), sizeof(packet), 0,
               reinterpret_cast<struct sockaddr*>(addr), sizeof(sockaddr_in));

    return bytesSent > 0;
}

// ============================================================================
// Receiving
// ============================================================================

std::vector<SpectrumBroadcaster::RemoteSpectrum> SpectrumBroadcaster::getReceivedSpectrums() {
    std::vector<RemoteSpectrum> results;
    int64_t now = getCurrentTimeMs();

    std::lock_guard<std::mutex> lock(receiveMutex);

    // Collect all non-stale entries and prune stale ones
    auto it = latestSpectrums.begin();
    while (it != latestSpectrums.end()) {
        if (now - it->second.timestamp > STALE_TIMEOUT_MS) {
            it = latestSpectrums.erase(it); // Prune stale entry
        } else {
            results.push_back(it->second);
            ++it;
        }
    }

    return results;
}

int SpectrumBroadcaster::getNumRemoteInstances() const {
    std::lock_guard<std::mutex> lock(receiveMutex);
    return static_cast<int>(latestSpectrums.size());
}

void SpectrumBroadcaster::receiverThreadRun() {
    SpectrumPacket packet;

    while (running.load()) {
        if (!receiveEnabled.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Receive packet (blocking with 100ms timeout set in socket options)
        int bytesReceived =
            recvfrom(recvSocket, reinterpret_cast<char*>(&packet), sizeof(packet), 0, nullptr, nullptr);

        if (bytesReceived < static_cast<int>(sizeof(SpectrumPacket) - MAX_SPECTRUM_BINS)) {
            // Not enough data for a valid header — timeout or truncated packet
            continue;
        }

        // Validate packet
        if (packet.magic != PROTOCOL_MAGIC) {
            continue; // Not our protocol
        }

        // Ignore our own broadcasts
        if (packet.instanceID == instanceID) {
            continue;
        }

        // Validate bin count
        if (packet.numBins == 0 || packet.numBins > MAX_SPECTRUM_BINS) {
            continue;
        }

        // Decompress and store in map
        RemoteSpectrum spectrum;
        spectrum.instanceID = packet.instanceID;
        spectrum.timestamp = getCurrentTimeMs(); // Use local time for staleness check
        spectrum.sampleRate = packet.sampleRate;
        decompressSpectrum(packet.magnitudes, packet.numBins, spectrum.magnitudes);

        {
            std::lock_guard<std::mutex> lock(receiveMutex);
            latestSpectrums[packet.instanceID] = std::move(spectrum);
        }
    }
}

// ============================================================================
// Spectrum Compression (dB-domain 8-bit quantization)
// ============================================================================

void SpectrumBroadcaster::compressSpectrum(const float* input, int inputBins, uint8_t* output,
                                           int outputBins) {
    const float dbRange = DB_CEILING - DB_FLOOR; // 80 dB

    if (inputBins <= outputBins) {
        // No downsampling needed — just dB-quantize each bin
        for (int i = 0; i < inputBins; ++i) {
            float magnitude = (std::max)(input[i], 1e-9f);
            float dB = 20.0f * std::log10(magnitude);
            float normalized = (dB - DB_FLOOR) / dbRange; // [0, 1] for [-80, 0] dB
            normalized = (std::max)(0.0f, (std::min)(1.0f, normalized));
            output[i] = static_cast<uint8_t>(normalized * 255.0f);
        }
        // Fill remaining bins with zero (silence)
        for (int i = inputBins; i < outputBins; ++i) {
            output[i] = 0;
        }
    } else {
        // Downsample by averaging bins (in linear domain), then dB-quantize
        float binRatio = static_cast<float>(inputBins) / static_cast<float>(outputBins);
        for (int i = 0; i < outputBins; ++i) {
            float start = static_cast<float>(i) * binRatio;
            float end = static_cast<float>(i + 1) * binRatio;
            int startBin = static_cast<int>(start);
            int endBin = (std::min)(static_cast<int>(std::ceil(end)), inputBins);

            // Average bins in this range (linear domain)
            float sum = 0.0f;
            int count = 0;
            for (int j = startBin; j < endBin; ++j) {
                sum += input[j];
                ++count;
            }

            float magnitude = (count > 0) ? (sum / static_cast<float>(count)) : 1e-9f;
            magnitude = (std::max)(magnitude, 1e-9f);
            float dB = 20.0f * std::log10(magnitude);
            float normalized = (dB - DB_FLOOR) / dbRange;
            normalized = (std::max)(0.0f, (std::min)(1.0f, normalized));
            output[i] = static_cast<uint8_t>(normalized * 255.0f);
        }
    }
}

void SpectrumBroadcaster::decompressSpectrum(const uint8_t* input, int numBins,
                                             std::vector<float>& output) {
    const float dbRange = DB_CEILING - DB_FLOOR; // 80 dB
    output.resize(numBins);

    for (int i = 0; i < numBins; ++i) {
        float normalized = static_cast<float>(input[i]) / 255.0f; // [0, 1]
        float dB = normalized * dbRange + DB_FLOOR;                // [-80, 0] dB
        output[i] = std::pow(10.0f, dB / 20.0f);                  // Back to linear
    }
}

} // namespace network
} // namespace phu
