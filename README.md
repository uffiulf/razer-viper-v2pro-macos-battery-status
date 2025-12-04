# Razer Viper V2 Pro Battery Monitor for macOS

A native macOS menu bar application to display battery status for the Razer Viper V2 Pro wireless mouse.

## Current Status: 🔧 Work in Progress

The application successfully communicates with the mouse via IOKit USB Control Transfers, but battery data is not yet being returned correctly.

---

## Technical Implementation

### Architecture
- **Backend**: C++ using macOS IOKit for USB communication
- **Frontend**: Objective-C++ using Cocoa (NSStatusBar)
- **Target Device**: Razer Viper V2 Pro (VID: 0x1532, PID: 0x00A6)

### Protocol Details (from librazermacos analysis)
```
Report Structure (90 bytes):
- Byte 0: Status (0x00 = New Command)
- Byte 1: Transaction ID (0xFF wired, 0x1F wireless)
- Bytes 2-4: Reserved
- Byte 5: Data Size (0x02)
- Byte 6: Command Class (0x07 = Power)
- Byte 7: Command ID (0x80 = Get Battery)
- Bytes 8-87: Arguments
- Byte 88: Checksum (XOR of bytes 2-87)
- Byte 89: Reserved
```

### USB Control Transfer Parameters
```
bmRequestType: 0x21 (SET) / 0xA1 (GET)
bRequest: 0x09 (SET_REPORT) / 0x01 (GET_REPORT)
wValue: 0x0300 (Feature Report, ID 0)
wIndex: 0x00 (protocol index for mice)
wLength: 90
```

---

## Latest Changes (Dec 4, 2025)

### 1. Switched from HIDAPI to IOKit
- HIDAPI Feature Reports were not working reliably
- Now using direct USB Control Transfers via `IOUSBInterfaceInterface`

### 2. Implemented Device Mode Initialization
- Added `setDeviceMode(0x03, 0x00)` to switch device to "Driver Mode"
- Device acknowledges the command with Status 0x00

### 3. Fixed wIndex Parameter
- **Previous**: `wIndex = 0x02` (USB Interface number)
- **Current**: `wIndex = 0x00` (protocol index per librazermacos)
- Mice use wIndex=0 by default, not the interface number

### 4. Matrix Test Implementation
- Systematically tests all combinations:
  - Transaction IDs: 0xFF, 0x1F, 0x3F
  - Commands: 0x80, 0x82

---

## Known Problem

### Symptom
The device responds with Status 0x00 (Success), but the response buffer is an **echo of the request** with empty data bytes:

```
Request:  00 FF 00 00 00 02 07 80 00 00 ...
Response: 00 FF 00 00 00 02 07 80 00 00 ... (Byte 9 = 0x00)
```

### Analysis
1. ✅ USB Interface 2 is correctly opened
2. ✅ Device Mode initialization succeeds
3. ✅ Commands are being received (Status 0x00)
4. ❓ Data payload is empty (device echoes command)

### Hypothesis
The Viper V2 Pro (0x00A6) is a newer device not supported by librazermacos. It may:
1. Use a different Transaction ID
2. Require a different initialization sequence
3. Have battery data at a different byte offset
4. Need a proprietary command not documented in OpenRazer

---

## Potential Fixes to Try

### 1. Try Interface 0 for ControlRequest
The USB Control Transfer might need to go through Interface 0 (default HID) instead of Interface 2.

### 2. Try Different wValue (Report ID)
Test wValue = 0x0301 or 0x0302 in case Viper V2 Pro uses a different Report ID.

### 3. Contact Razer Support
Request official HID protocol documentation for Viper V2 Pro battery queries.

### 4. USB Traffic Analysis
Use Wireshark with USBPcap on Windows (where Razer Synapse works) to capture the exact USB packets for battery queries.

---

## Building

```bash
# Requirements: Xcode Command Line Tools, hidapi (optional now)
make

# Run (requires sudo for USB access)
sudo ./RazerBatteryMonitor
```

---

## References

- [librazermacos](https://github.com/1kc/librazermacos) - Open source Razer driver for macOS
- [OpenRazer](https://github.com/openrazer/openrazer) - Linux Razer driver
- [Alex Perathoner's Razer Battery](https://github.com/alexanderperathoner/razer-battery-menu-bar-macos)

---

## License

MIT

