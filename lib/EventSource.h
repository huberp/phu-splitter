#pragma once

#include "Event.h"
#include <algorithm>
#include <vector>

/**
 * Base EventSource template
 *
 * Provides common listener management functionality for all event sources.
 * Eliminates code duplication between different EventSource types.
 *
 * @tparam ListenerType The listener interface type (e.g., GlobalsEventListener)
 */
template <typename ListenerType> class EventSource {
  protected:
    std::vector<ListenerType*> listeners;

  public:
    virtual ~EventSource() = default;

    /**
     * Add a listener
     * @param listener Pointer to listener (must outlive this EventSource or be removed)
     * @return The listener pointer (for chaining or storing the handle)
     */
    ListenerType* addEventListener(ListenerType* listener) {
        if (listener &&
            std::find(listeners.begin(), listeners.end(), listener) == listeners.end()) {
            listeners.push_back(listener);
        }
        return listener;
    }

    /**
     * Remove a listener
     * @param listener Pointer to listener to remove
     * @return true if listener was found and removed
     */
    bool removeEventListener(ListenerType* listener) {
        auto it = std::find(listeners.begin(), listeners.end(), listener);
        if (it != listeners.end()) {
            listeners.erase(it);
            return true;
        }
        return false;
    }

    /**
     * Get number of registered listeners
     */
    size_t getListenerCount() const {
        return listeners.size();
    }
};