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

## System Requirements

### Supported macOS Versions
| Version | Codename | Status |
|---------|----------|--------|
| macOS 15.x | Sequoia | ✅ Tested |
| macOS 14.x | Sonoma | ✅ Supported |
| macOS 13.x | Ventura | ✅ Supported |
| macOS 12.x | Monterey | ✅ Supported |
| macOS 11.x | Big Sur | ✅ Supported |
| macOS 10.15 | Catalina | ✅ Supported |
| macOS 10.14 | Mojave | ⚠️ Minimum |

### Supported Hardware
| Architecture | Status |
|--------------|--------|
| Apple Silicon (M1/M2/M3/M4) | ✅ Native (arm64) |
| Intel (x86_64) | ✅ Native (x86_64) |

**Note:** The DMG contains a **Universal Binary** that runs natively on both architectures.

### Other Requirements
- Xcode Command Line Tools (for building from source)
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

## 🔧 Porting to Other Razer Mice

This guide helps you adapt the app for other Razer wireless mice.

### Step 1: Find Your Mouse's USB IDs

```bash
# On macOS
ioreg -p IOUSB -l | grep -A10 "Razer"

# Look for:
# "idVendor" = 0x1532  (always same for Razer)
# "idProduct" = 0x00XX  (your mouse's PID)
```

**Note:** Many Razer mice have TWO PIDs - one for the wireless dongle and one for direct USB connection.

### Step 2: Modify `src/RazerDevice.hpp`

```cpp
// Change these constants to your mouse's PIDs:
static constexpr uint16_t PRODUCT_ID_DONGLE = 0x00XX;  // Wireless receiver PID
static constexpr uint16_t PRODUCT_ID_WIRED = 0x00YY;   // Wired connection PID
```

### Step 3: Test Transaction IDs

Different mice may require different Transaction IDs. Modify `queryBattery()` in `RazerDevice.cpp`:

```cpp
const uint8_t transIds[] = {0x1F, 0xFF, 0x3F};  // Try all common IDs
```

| Transaction ID | Typical Use |
|----------------|-------------|
| `0x1F` | Newer wireless mice (Viper V2 Pro, DeathAdder V3) |
| `0xFF` | Older wired mice |
| `0x3F` | Some Pro models |

### Step 4: Verify Response Bytes

Enable debug output and check which bytes contain data:

- **Byte 9**: Usually battery level (0-255 raw value)
- **Byte 11**: Usually charging status (0x01 = charging)

If your mouse uses different offsets, adjust in `queryBattery()` and `queryChargingStatus()`.

### Step 5: Known Razer Mouse PIDs

| Mouse | Wireless PID | Wired PID | Status |
|-------|-------------|-----------|--------|
| Viper V2 Pro | 0x00A6 | 0x00A5 | ✅ Tested |
| DeathAdder V3 Pro | 0x00B6 | 0x00B5 | 🔬 Untested |
| Basilisk V3 Pro | 0x00AA | 0x00A9 | 🔬 Untested |
| Viper Ultimate | 0x007A | 0x007B | 🔬 Untested |
| Naga V2 Pro | 0x00AD | ? | 🔬 Untested |

*PIDs may vary by region/revision. Always verify with `ioreg` command.*

### Step 6: Submit Your Changes

If you successfully port to another mouse:
1. Fork this repository
2. Add your mouse's PIDs and any protocol differences
3. Submit a Pull Request with your mouse model in the title

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Contributing

Pull requests welcome! If you port this to another Razer mouse, please share your findings to help others.
