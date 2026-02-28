#include "CommandBroadcaster.h"

#include <algorithm>
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
    #define CMD_INVALID_SOCKET INVALID_SOCKET
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #define CMD_INVALID_SOCKET (-1)
#endif

// Protocol magic: "CMND" in ASCII
static constexpr uint32_t COMMAND_MAGIC   = 0x434D4E44;
static constexpr uint16_t COMMAND_VERSION = 1;

// ============================================================================
// Construction
// ============================================================================

CommandBroadcaster::CommandBroadcaster()
    : MulticastBroadcasterBase(MULTICAST_GROUP, MULTICAST_PORT) {}

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

} // namespace network
} // namespace phu
