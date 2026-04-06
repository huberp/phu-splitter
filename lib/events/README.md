# C++ EventSource Implementation

This is a C++ translation of the Lua EventSource pattern used in ProtoplugScripts.

## Design Overview

**Pattern:** Observer Pattern with Multiple Interfaces (Option 3)

### Key Design Decisions

1. **Multiple Listener Interfaces** - Each event source has its own listener interface:
   - `GlobalsEventListener` - for BPM, IsPlaying, SampleRate events
   - `BufferEventListener` - for buffer configuration events

2. **Event Inheritance Hierarchy** - Events use polymorphic inheritance:
   - Base `Event` class with source and context
   - Derived classes: `BPMEvent`, `IsPlayingEvent`, `SampleRateEvent`, `BuffersChangedEvent`

3. **Type Safety** - Compile-time type checking ensures listeners only receive events they're designed for

4. **Multi-Source Listening** - Objects can implement multiple listener interfaces to subscribe to different sources

## File Structure

```
lib/
├── Event.h               # Event class hierarchy
├── EventSource.h         # EventSource implementations + listener registration
├── SyncGlobals.h         # Singleton GLOBALS (like Lua SyncGlobals)
├── SyncGlobalsListener.h # Listener interfaces (GlobalsEventListener, ...)
├── ExampleUsage.cpp      # Standalone usage examples
└── README.md             # This file
```

## Architecture Mapping

### Lua → C++ Translation

| Lua | C++ |
|-----|-----|
| `EventSource:new()` | `GlobalsEventSource` or `BufferEventSource` |
| `addEventListener(function)` | `addEventListener(ListenerInterface*)` |
| `removeEventListener(function)` | `removeEventListener(ListenerInterface*)` |
| `fireEvent(table)` | `fireBPMChanged(BPMEvent&)`, etc. |
| Event table `{type="BPM", ...}` | `BPMEvent` struct with members |

### Multi-Source Listening Example

**Lua:**
```lua
GLOBALS:addEventListener(function(inEvent) BUFFERS:listenToGlobalsChange(inEvent) end)
BUFFERS:addEventListener(function(inEvent) CLIENT_PATHS:listenToBufferChanges(inEvent) end)
```

**C++:**
```cpp
class BuffersManager : public GlobalsEventListener, public BufferEventSource {
    void onBPMChanged(const BPMEvent& event) override {
        // React to GLOBALS changes
        fireBuffersChanged(bufferEvent); // Notify our listeners
    }
};

BuffersManager buffers;
globals.addEventListener(&buffers);      // BUFFERS listens to GLOBALS
buffers.addEventListener(&clientPaths);  // CLIENT_PATHS listens to BUFFERS
```

## Usage Examples

### 1. Simple Listener

```cpp
class MyListener : public GlobalsEventListener {
    void onBPMChanged(const BPMEvent& event) override {
        std::cout << "BPM: " << event.newValues.bpm << std::endl;
    }
};

MyListener listener;
SyncGlobals::getInstance().addEventListener(&listener);
```

### 2. Multi-Source Listener

```cpp
class MyProcessor : public GlobalsEventListener, public BufferEventListener {
    void onBPMChanged(const BPMEvent& event) override {
        // Handle BPM changes
    }
    
    void onBuffersChanged(const BuffersChangedEvent& event) override {
        // Handle buffer changes
    }
};

MyProcessor processor;
globals.addEventListener(&processor);
buffers.addEventListener(&processor);
```

### 3. Event Chain (like Lua cascade)

```cpp
// Create the chain: GLOBALS → BUFFERS → CLIENT_PATHS → RMS
BuffersManager buffers;
ClientPathsManager clientPaths;
RMSCalculator rms;

// Wire up the chain
globals.addEventListener(&buffers);
buffers.addEventListener(&clientPaths);
buffers.addEventListener(&rms);

// Now when GLOBALS fires, events cascade down the chain
```

## Advantages Over Lua Version

1. **Type Safety** - Compile-time checking of event types and listener interfaces
2. **Performance** - No dynamic dispatch overhead from string type checking
3. **Memory Safety** - Clear lifetime management (vs. Lua GC)
4. **IDE Support** - Autocomplete and refactoring tools work
5. **Debugging** - Easier to set breakpoints and inspect state

## Compiling the Example

```bash
# With g++
g++ -std=c++17 -o example ExampleUsage.cpp

# With MSVC
cl /std:c++17 /EHsc ExampleUsage.cpp

# Run
./example  # or example.exe on Windows
```

## Integration with Audio Plugin

To integrate with a real audio plugin (VST, AU, etc.):

1. Call `SyncGlobals::getInstance().updateSampleRate()` in your plugin's `prepareToPlay()`
2. Call `SyncGlobals::getInstance().updateDAWGlobals()` at the start of each `processBlock()`
3. Implement `GlobalsEventListener` in your plugin components
4. Register listeners with `addEventListener()`
5. Clean up with `removeEventListener()` in destructors

In this repository, `ChordPatternCoordinator` implements `GlobalsEventListener` so it can react to transport/tempo/sample-rate changes routed through `SyncGlobals`.

## Thread Safety Note

This implementation is **not thread-safe**. If you need to fire events from multiple threads:

1. Add `std::mutex` to each EventSource
2. Lock before modifying listener list or firing events
3. Consider using `std::shared_ptr` for listener management

## Questions Answered

### Q: Is multi-source + listening to multiple sources possible?

**Yes!** Objects can implement multiple listener interfaces:

```cpp
class MultiSourceListener : public GlobalsEventListener, public BufferEventListener {
    // Implements callbacks from both interfaces
};
```

### Q: How to design Event objects? Union of structs?

**Use inheritance hierarchy** (as implemented):
- Clean, type-safe polymorphic design
- Each event type is a separate class
- Easy to extend with new event types
- Works naturally with virtual dispatch

Alternative approaches (not implemented but viable):
- `std::variant<BPMEvent, IsPlayingEvent, ...>` for zero-allocation
- Tagged union for C-style approach (not recommended)
