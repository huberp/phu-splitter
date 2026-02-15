#include "SpectrumBroadcaster.h"
#include <algorithm>
#include <cstring>
#include <random>

// Include socket headers only in implementation
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
static constexpr uint32_t PROTOCOL_VERSION = 1;

#ifdef _WIN32
bool SpectrumBroadcaster::wsaInitialized = false;
int SpectrumBroadcaster::wsaRefCount = 0;
std::mutex SpectrumBroadcaster::wsaMutex;
#endif

SpectrumBroadcaster::SpectrumBroadcaster()
    : sendSocket(INVALID_SOCKET_VALUE), recvSocket(INVALID_SOCKET_VALUE),
      multicastAddr(nullptr), networkInitialized(false), instanceID(0) {
    // Allocate multicast address structure
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
}

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

    // Enable multicast loopback (receive our own packets for testing)
    // Set to 0 in production to avoid receiving own broadcasts
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

    // Set receive timeout (100ms) to allow checking running flag
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

uint32_t SpectrumBroadcaster::generateInstanceID() {
    // Generate random instance ID (collision unlikely with 32-bit space)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

int64_t SpectrumBroadcaster::getCurrentTimeMs() const {
    return juce::Time::currentTimeMillis();
}

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
    packet.magic = PROTOCOL_MAGIC;
    packet.version = PROTOCOL_VERSION;
    packet.instanceID = instanceID;
    packet.timestamp = static_cast<uint64_t>(now);
    packet.sampleRate = sampleRate;

    // Compress spectrum (downsample and quantize)
    int outputBins = juce::jmin(numBins, MAX_SPECTRUM_BINS);
    packet.numBins = static_cast<uint16_t>(outputBins);
    compressSpectrum(magnitudes, numBins, packet.magnitudes, outputBins);

    // Send packet
    auto* addr = static_cast<sockaddr_in*>(multicastAddr);
    int bytesSent =
        sendto(sendSocket, reinterpret_cast<const char*>(&packet), sizeof(packet), 0,
               reinterpret_cast<struct sockaddr*>(addr), sizeof(sockaddr_in));

    return bytesSent == static_cast<int>(sizeof(packet));
}

std::vector<SpectrumBroadcaster::RemoteSpectrum> SpectrumBroadcaster::getReceivedSpectrums() {
    std::vector<RemoteSpectrum> results;

    int start1, size1, start2, size2;
    int numReady = receiveFifo.getNumReady();
    receiveFifo.prepareToRead(numReady, start1, size1, start2, size2);

    // Read all available spectrums
    for (int i = 0; i < size1; ++i) {
        results.push_back(spectrumBuffer[start1 + i]);
    }
    for (int i = 0; i < size2; ++i) {
        results.push_back(spectrumBuffer[start2 + i]);
    }

    receiveFifo.finishedRead(size1 + size2);
    return results;
}

void SpectrumBroadcaster::receiverThreadRun() {
    SpectrumPacket packet;

    while (running.load()) {
        if (!receiveEnabled.load()) {
            // Sleep briefly if receiving is disabled
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Receive packet (blocking with timeout)
        int bytesReceived =
            recvfrom(recvSocket, reinterpret_cast<char*>(&packet), sizeof(packet), 0, nullptr, nullptr);

        if (bytesReceived == static_cast<int>(sizeof(packet))) {
            // Validate packet
            if (packet.magic != PROTOCOL_MAGIC || packet.version != PROTOCOL_VERSION) {
                continue; // Invalid packet
            }

            // Ignore our own broadcasts
            if (packet.instanceID == instanceID) {
                continue;
            }

            // Write to FIFO if space available
            int start1, size1, start2, size2;
            receiveFifo.prepareToWrite(1, start1, size1, start2, size2);

            if (size1 > 0) {
                RemoteSpectrum& spectrum = spectrumBuffer[start1];
                spectrum.instanceID = packet.instanceID;
                spectrum.timestamp = packet.timestamp;
                spectrum.sampleRate = packet.sampleRate;
                decompressSpectrum(packet.magnitudes, packet.numBins, spectrum.magnitudes);

                receiveFifo.finishedWrite(1);
            }
            // If FIFO is full, drop the packet (old data)
        }
        // Timeout or error: continue loop to check running flag
    }
}

void SpectrumBroadcaster::compressSpectrum(const float* input, int inputBins, uint8_t* output,
                                           int outputBins) {
    // Downsample if needed (simple decimation with averaging)
    if (inputBins <= outputBins) {
        // No downsampling needed, just quantize
        for (int i = 0; i < inputBins; ++i) {
            float magnitude = std::clamp(input[i], 0.0f, 1.0f);
            output[i] = static_cast<uint8_t>(magnitude * 255.0f);
        }
        // Fill remaining bins with zero
        for (int i = inputBins; i < outputBins; ++i) {
            output[i] = 0;
        }
    } else {
        // Downsample by averaging bins
        float binRatio = static_cast<float>(inputBins) / static_cast<float>(outputBins);
        for (int i = 0; i < outputBins; ++i) {
            float start = static_cast<float>(i) * binRatio;
            float end = static_cast<float>(i + 1) * binRatio;
            int startBin = static_cast<int>(start);
            int endBin = juce::jmin(static_cast<int>(std::ceil(end)), inputBins);

            // Average bins in this range
            float sum = 0.0f;
            int count = 0;
            for (int j = startBin; j < endBin; ++j) {
                sum += input[j];
                ++count;
            }

            float magnitude = (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
            magnitude = std::clamp(magnitude, 0.0f, 1.0f);
            output[i] = static_cast<uint8_t>(magnitude * 255.0f);
        }
    }
}

void SpectrumBroadcaster::decompressSpectrum(const uint8_t* input, int numBins,
                                             std::vector<float>& output) {
    output.resize(numBins);
    for (int i = 0; i < numBins; ++i) {
        output[i] = static_cast<float>(input[i]) / 255.0f;
    }
}

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
