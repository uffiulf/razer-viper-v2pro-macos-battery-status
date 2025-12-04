# Razer Viper V2 Pro Battery Monitor for macOS

A native macOS menu bar application that displays battery status for the Razer Viper V2 Pro wireless mouse.

![Status: Working](https://img.shields.io/badge/Status-Working-brightgreen)
![Platform: macOS](https://img.shields.io/badge/Platform-macOS-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow)

## Features

- 🔋 Real-time battery percentage in menu bar
- ⚡ Charging indicator when USB cable connected
- 🎨 Color-coded battery (🔴 <20%, 🟠 21-30%, ⚪ >30%)
- 🔔 Low battery notifications (< 20%)
- 🔄 Auto-refresh every 30 seconds
- 🖱️ Hover tooltip shows device name
- 🍎 Native macOS app using Cocoa + IOKit
- 📦 DMG installer with drag-and-drop installation

---

## Installation

### From DMG (Recommended)
1. Download `RazerBatteryMonitor.dmg`
2. Open the DMG and drag the app to Applications
3. Right-click the app → "Open" (first time only, to bypass Gatekeeper)
4. Grant Input Monitoring permission if prompted

### From Source
```bash
git clone https://github.com/uffiulf/razer-viper-v2pro-macos-battery-status.git
cd razer-viper-v2pro-macos-battery-status
make
sudo ./RazerBatteryMonitor
```

### Build Release DMG
```bash
./create_release.sh
open RazerBatteryMonitor.dmg
```

---

## Usage

| State | Display |
|-------|---------|
| Wireless (battery OK) | `🖱️ 85%` |
| Wireless (low battery) | `🖱️ 15%` (red) |
| Charging via USB | `🖱️ 100% ⚡` |
| Device not found | `🖱️ Not Found` |

**Menu options:**
- **Refresh** - Force immediate battery update
- **Quit** - Exit the application

---

## The Problem & Solution

### The Challenge

The Razer Viper V2 Pro (PID: 0x00A6) is a newer device **not officially supported** by open-source Razer drivers like [librazermacos](https://github.com/1kc/librazermacos) or [OpenRazer](https://github.com/openrazer/openrazer). No documentation exists for its USB HID protocol.

### What Went Wrong (Initial Attempts)

1. **HIDAPI Failed**: Using `hid_write()` froze the mouse completely because it conflicts with the mouse's input endpoint.

2. **Feature Reports Echoed**: Switching to `hid_send_feature_report()` worked for communication, but the device returned empty data (0x00) at the expected battery byte.

3. **Wrong Interface**: We were sending USB Control Transfers with `wIndex = 0x02` (the USB interface number), but Razer mice expect `wIndex = 0x00` (protocol index).

4. **Status 0x02 Rejected**: Our code only accepted `Status 0x00` (Success) as valid. We were discarding responses with `Status 0x02` (Busy) even when they contained valid battery data!

### How We Found the Fix

We implemented a **Matrix Test** that systematically tested all combinations of:
- Transaction IDs: `0xFF` (Wired), `0x1F` (Wireless), `0x3F` (Pro)
- Commands: `0x80` (Get Battery), `0x82` (Get Charging Status)

The test revealed:

```
TEST 3: TransID 0x1F + Cmd 0x80
  Status: 0x02 (Busy)
  Byte 9: 0xFF (255)
```

The mouse was returning **valid data** (0xFF = 100% battery) but with Status `0x02` instead of `0x00`. Our code was rejecting this as a failure!

### The Solution

1. **Accept Status 0x02**: Wireless Razer devices often return `Status 0x02` (Busy/Data Ready) with valid data
2. **Handle Status 0x04**: When wired, returns "Command not supported" - assume charging
3. **Use Transaction ID 0x1F**: The wireless protocol ID works for Viper V2 Pro
4. **Use IOKit directly**: Replaced HIDAPI with macOS IOKit USB Control Transfers
5. **Correct wIndex**: Changed from `0x02` to `0x00` per librazermacos implementation

---

## Technical Details

### Protocol Structure (90 bytes)

```
Byte 0:     Status (0x00 = New Command)
Byte 1:     Transaction ID (0x1F for wireless)
Bytes 2-4:  Reserved
Byte 5:     Data Size (0x02)
Byte 6:     Command Class (0x07 = Power)
Byte 7:     Command ID (0x80 = Get Battery, 0x84 = Get Charging)
Bytes 8-87: Arguments (battery at byte 9)
Byte 88:    Checksum (XOR of bytes 2-87)
Byte 89:    Reserved
```

### USB Control Transfer

```
bmRequestType: 0x21 (SET) / 0xA1 (GET)
bRequest:      0x09 (SET_REPORT) / 0x01 (GET_REPORT)
wValue:        0x0300 (Feature Report, ID 0)
wIndex:        0x00 (protocol index for mice)
wLength:       90 bytes
```

### Key Discoveries

| Parameter | Wrong Value | Correct Value |
|-----------|-------------|---------------|
| Transport | HIDAPI | IOKit USB Control Transfer |
| wIndex | 0x02 (interface) | 0x00 (protocol) |
| Transaction ID | 0xFF (wired) | 0x1F (wireless) |
| Valid Status | 0x00 only | 0x00, 0x02, or 0x04 |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    main.mm (Objective-C++)              │
│  ┌─────────────────┐  ┌──────────────────────────────┐  │
│  │  NSStatusItem   │  │  NSTimer (30s polling)       │  │
│  │  (Menu Bar UI)  │  │  USB Hotplug Notifications   │  │
│  └────────┬────────┘  └──────────────┬───────────────┘  │
│           │                          │                  │
│           └──────────┬───────────────┘                  │
│                      ▼                                  │
│           ┌──────────────────────┐                      │
│           │    RazerDevice.cpp   │                      │
│           │  - queryBattery()    │                      │
│           │  - queryChargingStatus() │                  │
│           └──────────┬───────────┘                      │
│                      │                                  │
└──────────────────────┼──────────────────────────────────┘
                       ▼
              ┌────────────────┐
              │  IOKit (macOS) │
              │  USB Control   │
              │   Transfers    │
              └────────┬───────┘
                       ▼
              ┌────────────────┐
              │  Razer Viper   │
              │   V2 Pro       │
              │  (Interface 2) │
              └────────────────┘
```

---

## Files

| File | Description |
|------|-------------|
| `src/RazerDevice.cpp` | USB communication via IOKit |
| `src/RazerDevice.hpp` | Header with constants and class definition |
| `src/main.mm` | Cocoa UI (NSStatusBar menu bar app) |
| `Info.plist` | macOS app configuration |
| `Makefile` | Build configuration |
| `build_app.sh` | Creates .app bundle |
| `create_release.sh` | Creates styled DMG installer |

---

## Requirements

- macOS 10.14+ (Mojave or later)
- Xcode Command Line Tools
- Razer Viper V2 Pro mouse
- `create-dmg` (for building DMG): `brew install create-dmg`

---

## Troubleshooting

### "Razer: Not Found"
- Ensure the mouse is connected (wired or via USB receiver)
- Check that no other app is claiming the device

### No menu bar icon
- Run as `.app` bundle, not raw binary
- Check Activity Monitor for running process

### Permission errors
- Grant Input Monitoring permission in System Settings
- First launch may require: right-click → Open

---

## References

- [librazermacos](https://github.com/1kc/librazermacos) - Key protocol reference
- [OpenRazer](https://github.com/openrazer/openrazer) - Linux Razer driver
- [Alex Perathoner's Razer Battery](https://github.com/alexanderperathoner/razer-battery-menu-bar-macos)

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Contributing

Pull requests welcome! The protocol details documented here may help support other Razer devices.
