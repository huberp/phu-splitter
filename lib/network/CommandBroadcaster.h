#pragma once

#include "MulticastBroadcasterBase.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace phu {
namespace network {

// ============================================================================
// Command Types
// ============================================================================

/**
 * Well-known command type identifiers.
 * User-defined commands can use values >= 0x1000.
 */
enum class CommandType : uint16_t {
    Solo   = 0x0001,    ///< Set band solo state
    Mute   = 0x0002,    ///< Set band mute state
    // Future commands go here...
};

// ============================================================================
// Command Packets (wire format)
// ============================================================================

/** Maximum payload size in bytes. */
static constexpr uint16_t MAX_COMMAND_PAYLOAD = 256;

/** Target group name that matches all peers. */
static constexpr const char* CMD_TARGET_ALL = "all";

#pragma pack(push, 1)
/**
 * Wire-format command packet.
 *
 * Layout:
 *   magic (4)  | version (2) | instanceID (4) | timestamp (8)
 *   commandType (2) | targetGroup (32) | payloadSize (2) | payload (256)
 *
 * Total fixed size: 310 bytes (fits comfortably in a single UDP datagram).
 */
struct CommandPacket {
    uint32_t magic;                         ///< Protocol magic: 0x434D4E44 ("CMND")
    uint16_t version;                       ///< Protocol version (currently 1)
    uint32_t instanceID;                    ///< Sender's unique instance ID
    uint64_t timestamp;                     ///< Sender's timestamp (ms since epoch)
    uint16_t commandType;                   ///< Discriminator (see CommandType enum)
    char     targetGroup[32];               ///< Target group name (null-terminated, e.g. "all")
    uint16_t payloadSize;                   ///< Actual payload bytes used (0..MAX_COMMAND_PAYLOAD)
    uint8_t  payload[MAX_COMMAND_PAYLOAD];  ///< Command-specific payload
};
#pragma pack(pop)

// ============================================================================
// Solo / Mute Payload
// ============================================================================

#pragma pack(push, 1)
/**
 * Payload for Solo and Mute commands.
 */
struct SoloMutePayload {
    uint8_t bandIndex;  ///< 0-based band index
    uint8_t state;      ///< 1 = on (solo/mute), 0 = off (unsolo/unmute)
};
#pragma pack(pop)

// ============================================================================
// CommandListener — interface for receiving commands
// ============================================================================

/**
 * Implement this interface to receive decoded commands from CommandBroadcaster.
 *
 * Callbacks are invoked on the receiver background thread.  If the listener
 * needs to touch the UI or parameter tree, it must marshal the call to the
 * appropriate thread (e.g. via MessageManager::callAsync).
 */
class CommandListener {
  public:
    virtual ~CommandListener() = default;

    /**
     * Called when a valid command arrives from a remote peer.
     *
     * @param commandType   The command discriminator.
     * @param senderID      Instance ID of the sender.
     * @param targetGroup   Target group string from the packet.
     * @param payload       Pointer to the raw payload bytes.
     * @param payloadSize   Number of payload bytes.
     */
    virtual void onCommandReceived(CommandType commandType,
                                   uint32_t senderID,
                                   const std::string& targetGroup,
                                   const uint8_t* payload,
                                   uint16_t payloadSize) = 0;
};

// ============================================================================
// CommandBroadcaster
// ============================================================================

/**
 * CommandBroadcaster: UDP multicast broadcaster/receiver for sharing
 * control commands between plugin instances in a DAW project.
 *
 * Architecture mirrors SpectrumBroadcaster but transmits discrete commands
 * rather than continuous data.  Uses a separate port to avoid interference
 * with the spectrum data stream.
 *
 * Thread safety:
 * - sendCommand()        is safe to call from any single thread.
 * - addListener()        must be called before initialize() or from the
 *                        message thread when the receiver is not dispatching.
 * - removeListener()     same as addListener().
 * - The receiver thread dispatches to CommandListener objects; listeners must
 *   be thread-safe or marshal internally.
 */
class CommandBroadcaster : public MulticastBroadcasterBase {
  public:
    /** Uses the same multicast group as SpectrumBroadcaster. */
    static constexpr const char* MULTICAST_GROUP = "239.255.42.1";

    /** Separate port for command channel. */
    static constexpr int MULTICAST_PORT = 49422;

    CommandBroadcaster();
    ~CommandBroadcaster() override = default;

    // Non-copyable / non-movable
    CommandBroadcaster(const CommandBroadcaster&) = delete;
    CommandBroadcaster& operator=(const CommandBroadcaster&) = delete;

    // ---- Sending ----------------------------------------------------------

    /**
     * Send a command to all peers (or a specific target group).
     *
     * @param type         Command discriminator.
     * @param payload      Command-specific payload bytes (may be nullptr if payloadSize == 0).
     * @param payloadSize  Size of payload in bytes (must be <= MAX_COMMAND_PAYLOAD).
     * @param targetGroup  Target group name (default: "all").
     * @return true if the packet was sent successfully.
     */
    bool sendCommand(CommandType type,
                     const uint8_t* payload = nullptr,
                     uint16_t payloadSize = 0,
                     const char* targetGroup = CMD_TARGET_ALL);

    // ---- Convenience senders ----------------------------------------------

    /** Send a Solo command for a specific band. */
    bool sendSoloCommand(uint8_t bandIndex, bool solo,
                         const char* targetGroup = CMD_TARGET_ALL);

    /** Send a Mute command for a specific band. */
    bool sendMuteCommand(uint8_t bandIndex, bool mute,
                         const char* targetGroup = CMD_TARGET_ALL);

    // ---- Listener management ----------------------------------------------

    void addListener(CommandListener* listener);
    void removeListener(CommandListener* listener);

    // ---- Configuration ----------------------------------------------------

    /** Set the peer group this instance belongs to (for filtering). */
    void setOwnGroup(const std::string& group) { ownGroup = group; }
    const std::string& getOwnGroup() const { return ownGroup; }

  protected:
    // MulticastBroadcasterBase overrides
    void receiverThreadRun() override;

  private:
    // Peer group this instance belongs to (default "all")
    std::string ownGroup{"all"};

    // Listeners
    std::mutex listenerMutex;
    std::vector<CommandListener*> listeners;
};

} // namespace network
} // namespace phu
