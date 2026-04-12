#ifndef NDEBUG // Debug builds only

#include "EditorLogger.h"
#include "../../src/PluginEditor.h"
#include <cstring>

namespace phu {
namespace debug {

void EditorLogger::setEditor(PhuSplitterAudioProcessorEditor* newEditor) {
    editor = newEditor;

    // If we accumulated messages before the editor existed, flush them now.
    requestAsyncUpdate();
}

void EditorLogger::clearEditor() {
    editor = nullptr;
}

void EditorLogger::markCurrentThreadAsAudioThread() noexcept {
    const auto id = reinterpret_cast<uintptr_t>(juce::Thread::getCurrentThreadId());
    audioThreadId.store(id, std::memory_order_relaxed);
}

void EditorLogger::requestAsyncUpdate() noexcept {
    // Ensure we only post one pending async update at a time.
    if (!asyncUpdateRequested.exchange(true, std::memory_order_acq_rel))
        triggerAsyncUpdate();
}

void EditorLogger::pushRealtime(const juce::String& message) noexcept {
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    rtFifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 == 0) {
        rtDroppedMessages.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto& slot = rtSlots[static_cast<size_t>(start1)];
    const char* utf8 = message.toRawUTF8();

    // Copy into fixed buffer (truncate if needed).
    const size_t maxCopy = rtMaxMessageBytes - 1;
    const size_t len = std::min(maxCopy, std::strlen(utf8));
    std::memcpy(slot.text.data(), utf8, len);
    slot.text[len] = '\0';
    slot.length = static_cast<uint16_t>(len);

    rtFifo.finishedWrite(1);
}

void EditorLogger::logMessage(const juce::String& message) {
    // This can be called from any thread.
    // Audio thread: lock-free SPSC queue (single producer).
    // Other threads: locked queue (not real-time critical).
    const auto currentThread = reinterpret_cast<uintptr_t>(juce::Thread::getCurrentThreadId());
    const auto audioThread = audioThreadId.load(std::memory_order_relaxed);

    if (audioThread != 0 && currentThread == audioThread) {
        pushRealtime(message);
    } else {
        const juce::ScopedLock lock(nonRealtimeLock);
        pendingMessages.add(message);
    }

    requestAsyncUpdate();
}

void EditorLogger::handleAsyncUpdate() {
    // This is called on the message thread
    asyncUpdateRequested.store(false, std::memory_order_release);

    // If no editor is attached, keep messages queued but don't spin the message loop.
    if (editor == nullptr)
        return;

    // Send all messages to the editor
    // 1) Drain realtime (audio-thread) queue first
    for (;;) {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        const int ready = rtFifo.getNumReady();
        if (ready <= 0)
            break;

        rtFifo.prepareToRead(ready, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i) {
            const auto& slot = rtSlots[static_cast<size_t>(start1 + i)];
            editor->addLogMessage(juce::String::fromUTF8(slot.text.data(), slot.length));
        }
        for (int i = 0; i < size2; ++i) {
            const auto& slot = rtSlots[static_cast<size_t>(start2 + i)];
            editor->addLogMessage(juce::String::fromUTF8(slot.text.data(), slot.length));
        }

        rtFifo.finishedRead(size1 + size2);
    }

    // Emit a dropped-message marker if needed
    const uint32_t dropped = rtDroppedMessages.exchange(0, std::memory_order_relaxed);
    if (dropped > 0)
        editor->addLogMessage("[Logger] Dropped " + juce::String(dropped) +
                              " realtime log messages");

    // 2) Drain non-realtime queue
    juce::StringArray messages;
    {
        const juce::ScopedLock lock(nonRealtimeLock);
        messages.swapWith(pendingMessages);
    }

    for (const auto& msg : messages)
        editor->addLogMessage(msg);

    // If more messages arrived while we were draining, schedule another update.
    if (rtFifo.getNumReady() > 0)
        requestAsyncUpdate();
    else {
        const juce::ScopedLock lock(nonRealtimeLock);
        if (pendingMessages.size() > 0)
            requestAsyncUpdate();
    }
}

} // namespace debug
} // namespace phu

#endif // !NDEBUG
