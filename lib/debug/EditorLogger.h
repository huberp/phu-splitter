#pragma once

#ifndef NDEBUG // Debug builds only

#include <array>
#include <atomic>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Forward declaration (PhuSplitterAudioProcessorEditor is in global namespace, not phu::debug)
class PhuSplitterAudioProcessorEditor;

namespace phu {
namespace debug {

/**
 * EditorLogger
 *
 * Custom JUCE Logger that forwards log messages to the plugin editor's log view.
 * Thread-safe and uses AsyncUpdater to ensure GUI updates happen on the message thread.
 *
 * Usage:
 *   // Call the instance logger directly (do NOT install it as the global JUCE logger)
 *   editorLogger->logMessage("Your message");
 *   // or
 *   LOG_MESSAGE(editorLogger, "Your message");
 */
class EditorLogger : public juce::Logger, public juce::AsyncUpdater {
  public:
    EditorLogger() = default;
    ~EditorLogger() override = default;

    /**
     * Mark the calling thread as the audio thread for this plugin instance.
     *
     * This enables lock-free, allocation-free queueing of log messages from the audio thread
     * (SPSC: audio thread producer -> message thread consumer).
     */
    void markCurrentThreadAsAudioThread() noexcept;

    /**
     * Set the editor that will receive log messages
     * @param editor Pointer to the editor (uses SafePointer internally)
     */
    void setEditor(PhuSplitterAudioProcessorEditor* editor);

    /**
     * Clear the editor reference (call when editor is destroyed)
     */
    void clearEditor();

    /**
     * Logger override - called from any thread
     * Adds message to queue and triggers async update
     */
    void logMessage(const juce::String& message) override;

  protected:
    /**
     * AsyncUpdater override - called on message thread
     * Flushes pending messages to the editor
     */
    void handleAsyncUpdate() override;

  private:
    // ---------------------------------------------------------------------
    // Real-time queue (SPSC): only the audio thread is allowed to push here.
    // ---------------------------------------------------------------------
    static constexpr int rtQueueCapacity = 1024;
    static constexpr size_t rtMaxMessageBytes = 256;

    struct RtSlot {
        std::array<char, rtMaxMessageBytes> text{};
        uint16_t length = 0;
    };

    juce::AbstractFifo rtFifo{rtQueueCapacity};
    std::array<RtSlot, rtQueueCapacity> rtSlots;
    std::atomic<uint32_t> rtDroppedMessages{0};

    std::atomic<uintptr_t> audioThreadId{0};

    // ---------------------------------------------------------------------
    // Non-real-time queue: safe for any thread, uses a lock.
    // ---------------------------------------------------------------------
    juce::CriticalSection nonRealtimeLock;
    juce::StringArray pendingMessages;

    // Coalesce async updates so we don't spam the message queue.
    std::atomic<bool> asyncUpdateRequested{false};

    // Safe pointer to editor (automatically nulled when editor is destroyed)
    juce::Component::SafePointer<PhuSplitterAudioProcessorEditor> editor;

    void requestAsyncUpdate() noexcept;
    void pushRealtime(const juce::String& message) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorLogger)
};

// Convenience macro for instance-scoped logging
#define LOG_MESSAGE(loggerPtr, msg)                                                                \
    do {                                                                                           \
        if ((loggerPtr) != nullptr)                                                                \
            (loggerPtr)->logMessage(msg);                                                          \
    } while (0)

} // namespace debug
} // namespace phu

#else // Release builds

// No-op macro for release builds
#define LOG_MESSAGE(loggerPtr, msg)                                                                \
    do {                                                                                           \
        (void)(loggerPtr);                                                                         \
        (void)(msg);                                                                               \
    } while (0)

#endif // !NDEBUG
