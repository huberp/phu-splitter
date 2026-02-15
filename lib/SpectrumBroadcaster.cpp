#include "SpectrumBroadcaster.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>

// Include socket headers only in implementation file
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
#endif

// Protocol magic number: "SPEC" in ASCII
static constexpr uint32_t PROTOCOL_MAGIC = 0x53504543;
static constexpr uint32_t PROTOCOL_VERSION = 2;

#ifdef _WIN32
bool SpectrumBroadcaster::wsaInitialized = false;
int SpectrumBroadcaster::wsaRefCount = 0;
std::mutex SpectrumBroadcaster::wsaMutex;
#endif

// ============================================================================
// Construction / Destruction
// ============================================================================

SpectrumBroadcaster::SpectrumBroadcaster()
    : sendSocket(INVALID_SOCKET_VALUE), recvSocket(INVALID_SOCKET_VALUE),
      multicastAddr(nullptr), networkInitialized(false), instanceID(0) {
    // Allocate multicast address structure (hidden behind void* in header)
    multicastAddr = new sockaddr_in();
    std::memset(multicastAddr, 0, sizeof(sockaddr_in));

    // Generate unique instance ID
    instanceID = generateInstanceID();
}

SpectrumBroadcaster::~SpectrumBroadcaster() {
    shutdown();

    // Free multicast address structure
    if (multicastAddr) {
        delete static_cast<sockaddr_in*>(multicastAddr);
        multicastAddr = nullptr;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SpectrumBroadcaster::initialize() {
    if (networkInitialized) {
        return true; // Already initialized
    }

#ifdef _WIN32
    if (!initializeWSA()) {
        return false;
    }
#endif

    if (!initializeSockets()) {
        shutdown();
        return false;
    }

    // Start receiver thread
    running.store(true);
    receiverThread = std::make_unique<std::thread>(&SpectrumBroadcaster::receiverThreadRun, this);

    networkInitialized = true;
    return true;
}

void SpectrumBroadcaster::shutdown() {
    // Stop receiver thread
    running.store(false);
    if (receiverThread && receiverThread->joinable()) {
        receiverThread->join();
    }
    receiverThread.reset();

    // Cleanup sockets
    cleanupSockets();

#ifdef _WIN32
    cleanupWSA();
#endif

    networkInitialized = false;

    // Clear received spectrums
    {
        std::lock_guard<std::mutex> lock(receiveMutex);
        latestSpectrums.clear();
    }
}

// ============================================================================
// Socket Setup
// ============================================================================

bool SpectrumBroadcaster::initializeSockets() {
    // Create sending socket
    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSocket == INVALID_SOCKET_VALUE) {
        return false;
    }

    // Set up multicast address
    auto* addr = static_cast<sockaddr_in*>(multicastAddr);
    std::memset(addr, 0, sizeof(sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(MULTICAST_PORT);
    inet_pton(AF_INET, MULTICAST_GROUP, &addr->sin_addr);

    // Enable multicast loopback (receive our own packets — filtered by instanceID)
    int loopback = 1;
    setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
               reinterpret_cast<const char*>(&loopback), sizeof(loopback));

    // Set multicast TTL (1 = local network only)
    int ttl = 1;
    setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_TTL,
               reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    // Create receiving socket
    recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recvSocket == INVALID_SOCKET_VALUE) {
        closesocket(sendSocket);
        sendSocket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Enable address reuse (multiple instances on same machine)
    int reuse = 1;
    setsockopt(recvSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

#ifdef SO_REUSEPORT
    // On Unix-like systems, also set SO_REUSEPORT
    setsockopt(recvSocket, SOL_SOCKET, SO_REUSEPORT,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

    // Bind to multicast port
    struct sockaddr_in bindAddr;
    std::memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(MULTICAST_PORT);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(recvSocket, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) ==
        SOCKET_ERROR_VALUE) {
        closesocket(sendSocket);
        closesocket(recvSocket);
        sendSocket = INVALID_SOCKET_VALUE;
        recvSocket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Join multicast group
    struct ip_mreq mreq;
    inet_pton(AF_INET, MULTICAST_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(recvSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR_VALUE) {
        closesocket(sendSocket);
        closesocket(recvSocket);
        sendSocket = INVALID_SOCKET_VALUE;
        recvSocket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Set receive timeout (100ms) to allow periodic checking of the running flag
#ifdef _WIN32
    DWORD timeout = 100;
    setsockopt(recvSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms
    setsockopt(recvSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif

    return true;
}

void SpectrumBroadcaster::cleanupSockets() {
    if (sendSocket != INVALID_SOCKET_VALUE) {
        closesocket(sendSocket);
        sendSocket = INVALID_SOCKET_VALUE;
    }
    if (recvSocket != INVALID_SOCKET_VALUE) {
        closesocket(recvSocket);
        recvSocket = INVALID_SOCKET_VALUE;
    }
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

// ============================================================================
// Helpers
// ============================================================================

uint32_t SpectrumBroadcaster::generateInstanceID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

int64_t SpectrumBroadcaster::getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Windows-specific WSA initialization
// ============================================================================

#ifdef _WIN32
bool SpectrumBroadcaster::initializeWSA() {
    std::lock_guard<std::mutex> lock(wsaMutex);
    if (!wsaInitialized) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            return false;
        }
        wsaInitialized = true;
    }
    ++wsaRefCount;
    return true;
}

void SpectrumBroadcaster::cleanupWSA() {
    std::lock_guard<std::mutex> lock(wsaMutex);
    if (wsaInitialized && --wsaRefCount == 0) {
        WSACleanup();
        wsaInitialized = false;
    }
}
#endif
