#include "CommandBroadcaster.h"

#include <algorithm>
#include <chrono>
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
    #define CMD_INVALID_SOCKET INVALID_SOCKET
    #define CMD_SOCKET_ERROR   SOCKET_ERROR
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define CMD_INVALID_SOCKET (-1)
    #define CMD_SOCKET_ERROR   (-1)
    #define closesocket close
#endif

// Protocol magic: "CMND" in ASCII
static constexpr uint32_t COMMAND_MAGIC   = 0x434D4E44;
static constexpr uint16_t COMMAND_VERSION = 1;

#ifdef _WIN32
bool  CommandBroadcaster::wsaInitialized = false;
int   CommandBroadcaster::wsaRefCount    = 0;
std::mutex CommandBroadcaster::wsaMutex;
#endif

// ============================================================================
// Construction / Destruction
// ============================================================================

CommandBroadcaster::CommandBroadcaster()
    : sendSocket(CMD_INVALID_SOCKET),
      recvSocket(CMD_INVALID_SOCKET),
      multicastAddr(nullptr),
      networkInitialized(false),
      instanceID(0) {
    multicastAddr = new sockaddr_in();
    std::memset(multicastAddr, 0, sizeof(sockaddr_in));
    instanceID = generateInstanceID();
}

CommandBroadcaster::~CommandBroadcaster() {
    shutdown();
    if (multicastAddr) {
        delete static_cast<sockaddr_in*>(multicastAddr);
        multicastAddr = nullptr;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool CommandBroadcaster::initialize() {
    if (networkInitialized)
        return true;

#ifdef _WIN32
    if (!initializeWSA())
        return false;
#endif

    if (!initializeSockets()) {
        shutdown();
        return false;
    }

    running.store(true);
    receiverThread = std::make_unique<std::thread>(&CommandBroadcaster::receiverThreadRun, this);

    networkInitialized = true;
    return true;
}

void CommandBroadcaster::shutdown() {
    running.store(false);
    if (receiverThread && receiverThread->joinable())
        receiverThread->join();
    receiverThread.reset();

    cleanupSockets();

#ifdef _WIN32
    cleanupWSA();
#endif

    networkInitialized = false;
}

// ============================================================================
// Socket Setup
// ============================================================================

bool CommandBroadcaster::initializeSockets() {
    // --- send socket -------------------------------------------------------
    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSocket == CMD_INVALID_SOCKET)
        return false;

    // Multicast destination address
    auto* addr = static_cast<sockaddr_in*>(multicastAddr);
    std::memset(addr, 0, sizeof(sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port   = htons(MULTICAST_PORT);
    inet_pton(AF_INET, MULTICAST_GROUP, &addr->sin_addr);

    // Enable loopback (filtered by instanceID on receive)
    int loopback = 1;
    setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
               reinterpret_cast<const char*>(&loopback), sizeof(loopback));

    // TTL = 1 (local network only)
    int ttl = 1;
    setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_TTL,
               reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    // --- receive socket ----------------------------------------------------
    recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recvSocket == CMD_INVALID_SOCKET) {
        closesocket(sendSocket);
        sendSocket = CMD_INVALID_SOCKET;
        return false;
    }

    // Address reuse
    int reuse = 1;
    setsockopt(recvSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(recvSocket, SOL_SOCKET, SO_REUSEPORT,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

    // Bind
    struct sockaddr_in bindAddr{};
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_port        = htons(MULTICAST_PORT);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(recvSocket, reinterpret_cast<struct sockaddr*>(&bindAddr),
             sizeof(bindAddr)) == CMD_SOCKET_ERROR) {
        closesocket(sendSocket);
        closesocket(recvSocket);
        sendSocket = CMD_INVALID_SOCKET;
        recvSocket = CMD_INVALID_SOCKET;
        return false;
    }

    // Join multicast group
    struct ip_mreq mreq{};
    inet_pton(AF_INET, MULTICAST_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(recvSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == CMD_SOCKET_ERROR) {
        closesocket(sendSocket);
        closesocket(recvSocket);
        sendSocket = CMD_INVALID_SOCKET;
        recvSocket = CMD_INVALID_SOCKET;
        return false;
    }

    // Receive timeout 100ms (so receiver thread can check the running flag)
#ifdef _WIN32
    DWORD timeout = 100;
    setsockopt(recvSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100000;
    setsockopt(recvSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif

    return true;
}

void CommandBroadcaster::cleanupSockets() {
    if (sendSocket != CMD_INVALID_SOCKET) {
        closesocket(sendSocket);
        sendSocket = CMD_INVALID_SOCKET;
    }
    if (recvSocket != CMD_INVALID_SOCKET) {
        closesocket(recvSocket);
        recvSocket = CMD_INVALID_SOCKET;
    }
}

// ============================================================================
// Sending
// ============================================================================

bool CommandBroadcaster::sendCommand(CommandType type,
                                     const uint8_t* payload,
                                     uint16_t payloadSize,
                                     const char* targetGroup) {
    if (!networkInitialized || sendSocket == CMD_INVALID_SOCKET)
        return false;

    if (payloadSize > MAX_COMMAND_PAYLOAD)
        return false;

    CommandPacket pkt{};
    pkt.magic       = COMMAND_MAGIC;
    pkt.version     = COMMAND_VERSION;
    pkt.instanceID  = instanceID;
    pkt.timestamp   = static_cast<uint64_t>(getCurrentTimeMs());
    pkt.commandType = static_cast<uint16_t>(type);
    pkt.payloadSize = payloadSize;

    // Copy target group (null-terminated, clamped to 31 chars)
#ifdef _WIN32
    strncpy_s(pkt.targetGroup, sizeof(pkt.targetGroup), targetGroup, _TRUNCATE);
#else
    std::strncpy(pkt.targetGroup, targetGroup, sizeof(pkt.targetGroup) - 1);
    pkt.targetGroup[sizeof(pkt.targetGroup) - 1] = '\0';
#endif

    if (payload && payloadSize > 0)
        std::memcpy(pkt.payload, payload, payloadSize);

    auto* addr = static_cast<sockaddr_in*>(multicastAddr);
    int bytesSent = sendto(sendSocket,
                           reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
                           reinterpret_cast<struct sockaddr*>(addr), sizeof(sockaddr_in));
    return bytesSent > 0;
}

bool CommandBroadcaster::sendSoloCommand(uint8_t bandIndex, bool solo,
                                         const char* targetGroup) {
    SoloMutePayload p{};
    p.bandIndex = bandIndex;
    p.state     = solo ? 1 : 0;
    return sendCommand(CommandType::Solo,
                       reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                       targetGroup);
}

bool CommandBroadcaster::sendMuteCommand(uint8_t bandIndex, bool mute,
                                         const char* targetGroup) {
    SoloMutePayload p{};
    p.bandIndex = bandIndex;
    p.state     = mute ? 1 : 0;
    return sendCommand(CommandType::Mute,
                       reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                       targetGroup);
}

// ============================================================================
// Listener Management
// ============================================================================

void CommandBroadcaster::addListener(CommandListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex);
    if (std::find(listeners.begin(), listeners.end(), listener) == listeners.end())
        listeners.push_back(listener);
}

void CommandBroadcaster::removeListener(CommandListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex);
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

// ============================================================================
// Receiver Thread
// ============================================================================

void CommandBroadcaster::receiverThreadRun() {
    CommandPacket pkt;

    while (running.load()) {
        int bytesReceived = recvfrom(recvSocket,
                                     reinterpret_cast<char*>(&pkt), sizeof(pkt),
                                     0, nullptr, nullptr);

        // Need at least the fixed header portion (everything before payload)
        constexpr int minHeaderSize = static_cast<int>(
            sizeof(CommandPacket) - MAX_COMMAND_PAYLOAD);

        if (bytesReceived < minHeaderSize)
            continue; // Timeout or truncated

        // Validate magic
        if (pkt.magic != COMMAND_MAGIC)
            continue;

        // Ignore own packets
        if (pkt.instanceID == instanceID)
            continue;

        // Validate version
        if (pkt.version != COMMAND_VERSION)
            continue;

        // Validate payload size
        if (pkt.payloadSize > MAX_COMMAND_PAYLOAD)
            continue;

        // Ensure targetGroup is null-terminated
        pkt.targetGroup[sizeof(pkt.targetGroup) - 1] = '\0';

        // Target group filtering: accept if target is "all" or matches our group
        std::string target(pkt.targetGroup);
        if (target != CMD_TARGET_ALL && target != ownGroup)
            continue;

        // Dispatch to listeners
        auto cmdType = static_cast<CommandType>(pkt.commandType);
        {
            std::lock_guard<std::mutex> lock(listenerMutex);
            for (auto* listener : listeners) {
                listener->onCommandReceived(cmdType, pkt.instanceID,
                                            target, pkt.payload, pkt.payloadSize);
            }
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================

uint32_t CommandBroadcaster::generateInstanceID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

int64_t CommandBroadcaster::getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Windows-specific WSA initialization
// ============================================================================

#ifdef _WIN32
bool CommandBroadcaster::initializeWSA() {
    std::lock_guard<std::mutex> lock(wsaMutex);
    if (!wsaInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            return false;
        wsaInitialized = true;
    }
    ++wsaRefCount;
    return true;
}

void CommandBroadcaster::cleanupWSA() {
    std::lock_guard<std::mutex> lock(wsaMutex);
    if (wsaInitialized && --wsaRefCount == 0) {
        WSACleanup();
        wsaInitialized = false;
    }
}
#endif
