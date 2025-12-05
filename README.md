# Razer Viper V2 Pro Battery Monitor for macOS

A native macOS menu bar application that displays battery status for the Razer Viper V2 Pro wireless mouse.

![Status: Working](https://img.shields.io/badge/Status-Working-brightgreen)
![Version: 1.2.0](https://img.shields.io/badge/Version-1.2.0-blue)
![Platform: macOS](https://img.shields.io/badge/Platform-macOS-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow)

## Features

- 🔋 Real-time battery percentage in menu bar
- ⚡ Charging indicator when USB cable connected (instant detection)
- 🎨 Color-coded battery levels:
  - 🔴 Red: ≤20% (Critical)
  - 🟡 Yellow: 21-30% (Warning)
  - 🟢 Green: >30% (Good)
- 🔔 Low battery notifications (< 20%)
- 🔄 Auto-refresh every 30 seconds + USB hotplug detection
- 🔌 Automatic Wired/Wireless mode detection via Product ID
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
| Wireless (battery OK) | `🖱️ 85%` (green) |
| Wireless (low battery) | `🖱️ 15%` (red) |
| Charging via USB | `🖱️ 100% ⚡` (green) |
| Device not found | `🖱️ Not Found` |

**Menu options:**
- **Refresh** - Force immediate battery update
- **Quit** - Exit the application

---

## How It Works

### Wired vs. Wireless Detection

The Razer Viper V2 Pro uses **different USB Product IDs** depending on connection type:

| Connection | Product ID (PID) | Mode |
|------------|------------------|------|
| USB Cable (Direct) | `0x00A5` (165) | Wired/Charging |
| USB Dongle (Wireless) | `0x00A6` (166) | Wireless |

The app detects which PID is present and automatically sets the charging status accordingly. When connected via cable (PID 0xA5), the ⚡ icon appears instantly.

### Battery Query Protocol

- **Command 0x80**: Get Battery Level (Byte 9 = 0-255 raw value)
- **Command 0x84**: Get Charging Status (Byte 11 = 0x01 if charging)
- **Transaction ID 0x1F**: Wireless protocol (works for Viper V2 Pro)

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
Bytes 8-87: Arguments (battery at byte 9, charging at byte 11)
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

| Parameter | Description |
|-----------|-------------|
| PID 0xA5 | Wired mouse (direct USB connection = charging) |
| PID 0xA6 | Wireless dongle |
| Transaction ID 0x1F | Works for Viper V2 Pro (not 0xFF) |
| Valid Status | 0x00, 0x02, or 0x04 (not just 0x00) |
| Battery Byte | Response byte 9 (0-255 scale) |
| Charging Byte | Response byte 11 (0x01 = charging) |

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
│           │  - PID-based mode detect │                  │
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
| `src/RazerDevice.cpp` | USB communication via IOKit, PID detection |
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

## Changelog

### v1.2.0
- **PID-based mode detection**: Instant wired/wireless detection using USB Product ID
  - PID 0xA5 = Wired (Charging)
  - PID 0xA6 = Wireless (Dongle)
- **Color-coded battery**: Red (≤20%), Yellow (21-30%), Green (>30%)
- **Charging status fix**: Correctly reads byte 11 for charging state
- **USB hotplug monitoring**: Detects cable connect/disconnect events

### v1.1.0
- IOKit USB Control Transfers (replaced HIDAPI)
- Driver Mode initialization for wireless devices
- Accepts Status 0x00, 0x02, and 0x04 responses

### v1.0.0
- Initial release with basic battery monitoring

---

## References

- [librazermacos](https://github.com/1kc/librazermacos) - Key protocol reference
- [OpenRazer](https://github.com/openrazer/openrazer) - Linux Razer driver

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Contributing

Pull requests welcome! The protocol details documented here may help support other Razer devices.
