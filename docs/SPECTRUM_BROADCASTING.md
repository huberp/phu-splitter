# Spectrum Broadcasting Feature

## Overview

The spectrum broadcasting feature enables multiple PHU-SPLITTER plugin instances running in the same DAW project to share their frequency spectrum data over the local network. This allows you to visualize all instances' spectrums simultaneously on each plugin's display, providing better context for mixing decisions.

## How It Works

### Network Protocol
- **Protocol**: UDP multicast (IPv4)
- **Multicast Address**: 239.255.42.1
- **Port**: 49421
- **Scope**: Local network only (TTL=1)
- **Data Format**: Compressed spectrum (512 bins max, 8-bit quantization)
- **Update Rate**: ~30Hz (configurable, throttled from UI's 60Hz)

### Architecture
```
┌─────────────────────────────────────────────────────┐
│ Plugin Instance A                                   │
│  ┌──────────────┐    ┌──────────────────┐         │
│  │ FFT (Output) │───▶│ Broadcaster      │──┐      │
│  └──────────────┘    │ (UI Thread)      │  │      │
│                      └──────────────────┘  │      │
│                                             │      │
│  ┌──────────────┐    ┌──────────────────┐  │      │
│  │ Crossover    │◀───│ Receiver Thread  │◀─┤      │
│  │ Frequency    │    │ (Lock-free FIFO) │  │      │
│  │ Bar          │    └──────────────────┘  │      │
│  └──────────────┘                          │      │
└──────────────────────────────────────────┼─┘      │
                                            │        │
                         UDP Multicast ─────┼────────┤
                         239.255.42.1:49421 │        │
┌──────────────────────────────────────────┼─┐      │
│ Plugin Instance B                         │        │
│  ┌──────────────┐    ┌──────────────────┐ │       │
│  │ FFT (Output) │───▶│ Broadcaster      │─┘       │
│  └──────────────┘    │ (UI Thread)      │         │
│                      └──────────────────┘         │
│                                                    │
│  ┌──────────────┐    ┌──────────────────┐        │
│  │ Crossover    │◀───│ Receiver Thread  │◀───────┘
│  │ Frequency    │    │ (Lock-free FIFO) │
│  │ Bar          │    └──────────────────┘
│  └──────────────┘
└─────────────────────────────────────────────────────┘
```

## Usage

### Enabling Broadcasting

1. **Open multiple plugin instances** in your DAW project
2. **Enable the Output FFT toggle** (to generate spectrum data)
3. **Click "Broadcast Spectrum"** checkbox to enable broadcasting
4. **Local spectrums** appear as **white lines** (thick=output, thin=input)
5. **Remote spectrums** from other instances appear as **colored lines**
   - Each instance gets a unique color based on its random ID
   - Up to 12 distinct colors, cycling through HSV hues

### Controls

| Control | Function |
|---------|----------|
| **Input FFT** | Display local input spectrum (white, thin) |
| **Output FFT** | Display local output spectrum (white, thick) |
| **Remote FFT** | Display remote instances' spectrums (colored) |
| **Broadcast Spectrum** | Enable/disable network broadcasting |

### Visual Indicators

- **White (thick)**: Your output spectrum
- **White (thin)**: Your input spectrum
- **Colored**: Other plugin instances
  - Each instance has a consistent color throughout the session
  - Colors cycle through: red, orange, yellow, green, cyan, blue, purple, etc.

## Technical Details

### Performance Characteristics

- **Bandwidth**: ~15-30 KB/sec per instance (at 30Hz, 512 bins)
- **CPU Usage**: Minimal (non-blocking I/O, lock-free queues)
- **Latency**: <50ms typical (network + processing)
- **Thread Model**:
  - UI thread: Broadcasts spectrum, consumes received data
  - Receiver thread: Blocking socket with 100ms timeout
  - Lock-free FIFO: Thread-safe communication (32 spectrum buffer)

### Data Compression

Original FFT data is compressed for network transmission:
1. **Downsampling**: 1024-16384 bins → 512 bins (averaging)
2. **Quantization**: 32-bit float → 8-bit uint (0-255)
3. **Result**: ~2KB per packet (header + 512 bytes)

### Packet Format

```cpp
struct SpectrumPacket {
    uint32_t magic;        // 0x53504543 ("SPEC")
    uint32_t version;      // Protocol version (1)
    uint32_t instanceID;   // Random unique ID
    uint64_t timestamp;    // Milliseconds since epoch
    uint16_t numBins;      // Number of bins (≤512)
    float sampleRate;      // Sample rate for frequency mapping
    uint8_t magnitudes[512]; // Quantized spectrum data
};
```

## Limitations & Considerations

### Current Limitations

1. **Local network only**: TTL=1 means broadcasts don't leave the local subnet
2. **UDP unreliable**: Occasional packet loss is acceptable (visualization degrades gracefully)
3. **No authentication**: Any device can join the multicast group
4. **Fixed multicast address**: All PHU-SPLITTER instances use the same address
5. **Windows Firewall**: May require allowing UDP port 49421

### Security Considerations

- **No encryption**: Spectrum data is sent in plaintext
- **No authentication**: Cannot verify sender identity
- **Local network exposure**: Any device on the network can receive broadcasts
- **Recommendation**: Use only on trusted networks (e.g., local studio network)

### DAW Compatibility

- ✅ **Works**: Most DAWs allow network access from plugins
- ⚠️ **May require permission**: Some sandboxed environments (e.g., macOS App Sandbox)
- ❌ **May not work**: Heavily sandboxed or restricted environments

## Troubleshooting

### No remote spectrums visible

1. **Check "Broadcast Spectrum" is enabled** on all instances
2. **Check "Remote FFT" toggle is enabled**
3. **Verify firewall allows UDP port 49421**
   - Windows: `netsh advfirewall firewall show rule name=all | findstr 49421`
   - Add rule: `netsh advfirewall firewall add rule name="PHU-SPLITTER Multicast" dir=in action=allow protocol=UDP localport=49421`
4. **Check debug log** (Debug builds only) for initialization errors
5. **Verify network supports multicast**
   - Some networks block multicast traffic
   - Test on localhost first (loopback enabled by default)

### High CPU usage

1. **Reduce broadcast rate**: Edit `SpectrumBroadcaster.cpp`, increase `minBroadcastIntervalMs`
2. **Disable when not needed**: Uncheck "Broadcast Spectrum"
3. **Use fewer instances**: Each instance adds ~1-2% CPU

### Packet loss / choppy display

1. **Normal behavior**: UDP can drop packets, visualization continues
2. **Severe loss**: Check network congestion, WiFi signal strength
3. **Workaround**: Use wired Ethernet instead of WiFi

## Implementation Notes

### For Developers

The implementation demonstrates several real-time audio programming patterns:

1. **Lock-free communication**: Uses JUCE's `AbstractFifo` for thread-safe data transfer
2. **Non-blocking networking**: Receiver thread with timeout prevents blocking UI
3. **Graceful degradation**: Dropped packets don't crash or stall the plugin
4. **Minimal allocation**: Pre-allocated buffers, no runtime allocation in hot paths
5. **Cross-platform**: Windows (WSA) and Unix (Berkeley sockets) support

### Future Enhancements

Potential improvements (not implemented):
- [ ] Configurable multicast address/port
- [ ] Instance naming (display instance labels)
- [ ] Spectrum history / persistence
- [ ] Peak hold / decay control
- [ ] Discovery protocol (automatic instance detection)
- [ ] Encryption / authentication (TLS/DTLS)
- [ ] Shared memory backend (same-machine optimization)
- [ ] Per-instance color customization

## References

- **JUCE Framework**: https://juce.com/
- **UDP Multicast**: RFC 1112, RFC 2365
- **Lock-free Programming**: JUCE AbstractFifo documentation
- **FFT Visualization**: See [lib/FFTProcessor.h](../lib/FFTProcessor.h)

## License

Same as the main project (see LICENSE file).
