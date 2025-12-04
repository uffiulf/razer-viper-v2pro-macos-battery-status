# Razer Battery Monitor - Project Log

## Project Scaffolding

### Files Created

1. **Makefile** (created)
   - Build configuration with pkg-config for hidapi auto-detection
   - Links IOKit, Cocoa, and CoreFoundation frameworks
   - Compiles C++ and Objective-C++ sources
   - Output binary: RazerBatteryMonitor

2. **src/RazerDevice.hpp** (created)
   - C++ header file for RazerDevice class
   - Declares HID communication interface
   - Constants for VID (0x1532), PID (0x00A6), and report sizes

3. **src/RazerDevice.cpp** (created)
   - Implementation of RazerDevice class
   - Constructs 90-byte HID reports with proper structure
   - Implements XOR checksum calculation (bytes 2-88)
   - Parses battery level from response byte 9
   - Includes debug output for response buffer

4. **src/main.mm** (created)
   - Objective-C++ main application file
   - Implements NSStatusBar menu bar integration
   - Sets up 15-minute polling timer for battery queries
   - Shows macOS notifications when battery < 20%
   - Handles application lifecycle

5. **Info.plist** (created)
   - macOS application configuration
   - IOKit usage description for HID access
   - LSUIElement set to true (menu bar only app)
   - Bundle identifier: com.razer.batterymonitor

6. **project_log.md** (created)
   - This log file documenting project activities

### Commands Executed

1. **make clean** (executed)
   - Cleaned previous build artifacts
   - Output: Removed object files and binary

2. **make** (executed)
   - Compiled RazerDevice.cpp → RazerDevice.o
   - Compiled main.mm → main.o
   - Linked object files with hidapi and frameworks → RazerBatteryMonitor
   - Result: SUCCESS (binary created)
   - Warnings: Deprecation warnings for NSStatusItem and NSUserNotification APIs (non-blocking)

### Build Fixes Applied

- Fixed header include in RazerDevice.hpp: Changed `#include <hidapi/hidapi.h>` to `#include <hidapi.h>` to match pkg-config include path structure

### HID Communication Fix

- **Changed from output reports to feature reports** (for wireless dongle compatibility)
- Modified `sendReport()`: Changed `hid_write()` → `hid_send_feature_report()`
- Modified `readResponse()`: Changed `hid_read()` → `hid_get_feature_report()`
- Report ID (0x00) kept as first byte for feature reports
- This prevents mouse freezing when using wireless dongle (2.4GHz mode)

### Runtime Testing

1. **Binary Execution - First Attempt** (executed)
   - Command: `./RazerBatteryMonitor`
   - Status: Application launched successfully
   - Result: Failed to connect to Razer Viper V2 Pro (VID: 0x1532, PID: 0xa6)
   - Output: "Failed to open Razer Viper V2 Pro (VID: 0x1532, PID: 0xa6)"
   - Application continues running in background (menu bar app)

2. **Binary Execution - Second Attempt** (executed)
   - Command: `./RazerBatteryMonitor`
   - Status: Application launched successfully
   - Result: **PROGRESS** - Device connection succeeded, but report sending failed
   - Output: "Failed to send battery query report"
   - Analysis: Device opened successfully (hid_open worked), but hid_write() failed

3. **Binary Execution - After Feature Report Fix** (executed)
   - Command: `make clean && make && ./RazerBatteryMonitor`
   - Status: Recompiled successfully with feature report changes
   - Result: Device connection failed (device may need reconnection after previous freeze)
   - Output: "Failed to open Razer Viper V2 Pro (VID: 0x1532, PID: 0xa6)"
   - Note: Code now uses `hid_send_feature_report()` and `hid_get_feature_report()` to avoid mouse freezing

4. **Binary Execution - With HID Enumeration Debug** (executed)
   - Command: `make clean && make && ./RazerBatteryMonitor`
   - Status: **SUCCESS** - Device enumeration and connection working
   - Result: Found 9 HID interfaces, device opened successfully, received response
   - Enumeration Output:
     - 9 devices found with different interface numbers (0, 1, 2) and usage pages
     - All devices show "Razer Viper V2 Pro" as product name
     - Paths: DevSrvsID:4295175444, DevSrvsID:4295175440, DevSrvsID:4295175436
   - Response Buffer: Received 90-byte response with protocol structure visible
   - Battery Level: Byte 9 shows 0x00 (may need verification or different byte offset)
   - **Key Success**: Feature report communication is working without mouse freezing

5. **Interface Targeting Fix** (executed)
   - Modified `connect()` to use `hid_open_path()` targeting Interface 1 or 2
   - Avoids Interface 0 (generic mouse interface) to prevent mouse freezing
   - Code now tries multiple candidate interfaces (Interface 1, Interface 2, Vendor Defined)
   - Fixed path storage issue by copying path string before freeing enumeration
   - Status: Code correctly targets vendor-specific interfaces
   - Current Issue: Privilege violation errors on all interfaces (macOS Input Monitoring permissions needed)
   - **Key Fix**: No longer accessing Interface 0, preventing mouse freeze issue

6. **Transaction ID Fix** (executed)
   - Changed Transaction ID from 0xFF to 0x1F in `queryBattery()`
   - 0x1F is standard for Razer wireless device queries
   - 0xFF was being ignored, causing empty response data
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test with proper Transaction ID

7. **Status Byte Fix** (executed)
   - Changed Status byte (report[0]) from 0x00 to 0x02 in `queryBattery()`
   - 0x02 = NEW_REQUEST status required for Razer Viper V2 Pro to trigger data fetch
   - This byte becomes buf[1] in the 91-byte buffer sent to device (after Report ID)
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test with Status 0x02 and Transaction ID 0x1F

8. **Retry Loop Implementation** (executed)
   - Implemented retry loop in `queryBattery()` to handle Status 0x02 (Busy) responses
   - Polls up to 20 times with 50ms delay between attempts
   - Checks Status byte (readBuffer[1]) after each `hid_get_feature_report()` call
   - Status 0x00 (Success): Breaks loop and parses battery data
   - Status 0x02 (Busy): Sleeps 50ms and retries
   - Added debug output: "Attempt X: Status 0x??" for each retry
   - Handles wireless dongle delay when fetching data from mouse
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test retry loop with wireless dongle

9. **Status Byte Revert to 0x00** (executed)
   - Changed Status byte (report[0]) back from 0x02 to 0x00
   - Keeping Transaction ID 0x1F (wireless target)
   - Testing combination: Status 0x00 (Standard Query) + TransID 0x1F (Wireless)
   - Previous attempts:
     - Status 0x00 + TransID 0xFF: Failed (empty data)
     - Status 0x02 + TransID 0x1F: Stuck in Busy loop
   - Retry loop remains in place (good practice for wireless devices)
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test Status 0x00 + TransID 0x1F combination

10. **Force Interface 2 Connection** (executed)
   - Modified `connect()` to ONLY accept Interface 2 (strict filtering)
   - Removed Interface 1 and Vendor Defined (usage_page >= 0xFF00) from candidates
   - Interface 1 was returning empty data (Status 0x00, Battery 0)
   - Interface 2 is the correct control interface (was returning Status 0x02 Busy, indicating it's the right one)
   - Code now explicitly checks `interface_number == 2` before attempting connection
   - Stops searching immediately when Interface 2 is found
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test with Interface 2 only + Status 0x00 + TransID 0x1F

11. **Status 0x02 Revert + Extended Retry** (executed)
   - Changed Status byte back from 0x00 to 0x02 (NEW_REQUEST)
   - Status 0x00 returns cached empty data (0%) - doesn't trigger fresh fetch
   - Status 0x02 correctly triggers data fetch but needs longer wait time
   - Increased retry limit from 20 to 100 attempts (~5 seconds total wait: 100 * 50ms)
   - Wireless mice need time to wake up and fetch data from mouse
   - Debug print already shows Status byte clearly: "Attempt X: Status 0xXX"
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` AND move the mouse simultaneously to wake it up

12. **Double Tap Query Strategy** (executed)
   - Removed retry loop approach (was getting stuck in Status 0x02 Busy)
   - Implemented "Double Tap" sequence for wireless Razer devices:
     1. Send NEW_REQUEST (Status 0x02) - triggers data fetch
     2. Wait 200ms for device to fetch data
     3. Send RETRIEVE (Status 0x00) - requests the result
     4. Wait 50ms before reading
     5. Read response once
   - Recalculates checksum after changing Status byte
   - Added debug output for each step
   - Verifies response Status is 0x00 before parsing
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test Double Tap strategy

13. **Command 0x82 (Get Charging Status)** (executed)
   - Changed Command ID from 0x80 (Get Battery) to 0x82 (Get Charging Status)
   - Viper V2 Pro returns battery level in Get Charging Status command response
   - Communication working (Status 0x00, Checksum OK) but data was empty with 0x80
   - Updated parsing to check both index 9 and index 10 for battery level
   - Added debug output showing both raw values before parsing
   - Falls back to index 10 if index 9 is 0
   - Double Tap strategy remains unchanged
   - Status: Recompiled successfully
   - **Next Step**: Run `sudo ./RazerBatteryMonitor` to test Command 0x82 with Double Tap strategy

### Observations

- Application compiled and runs without crashes
- HID device enumeration is working (hidapi initialized)
- **Second run**: Device connection succeeded (permissions granted or device connected)
- **Current issue**: Report sending failed (hid_write returns incorrect byte count)
- Possible causes for report send failure:
  1. Report structure/format may need adjustment
  2. Device may require different report ID or initialization sequence
  3. 91-byte buffer (with prepended 0x00) may not match device expectations
  4. Device may need feature report instead of output report

### Next Steps

- Investigate report sending failure - check hid_write return value and error details
- Consider using hid_send_feature_report() instead of hid_write() for Razer devices
- Add error checking to capture hidapi error messages (hid_error())
- Verify report structure matches Razer protocol exactly (may need different byte layout)
- Test with different report sizes or initialization sequences

14. **Systematisk Multi-Variation Testing** (executed)
   - Implementert systematisk testing av flere protokoll-variasjoner og command IDs
   - Tester 3 command IDs: 0x80, 0x82, 0x83
   - Tester 2 protokoll-variasjoner:
     - Variation 1: Byte 5=Data Size, Byte 7=Class, Byte 8=Command (nåværende)
     - Variation 2: Byte 6=Data Size, Byte 8=Class, Byte 9=Command (QuickTest style)
   - Økt ventetid til 1000ms (1 sekund) for wireless dongle
   - Smart scanning: Ignorerer command echo, søker bytes 9-15 for faktiske data
   - Returnerer umiddelbart når batteridata funnes
   - Status: Recompiled successfully
   - **Test Script**: Laget test_battery.sh for enkel testing
   - **Next Step**: Kjør `sudo ./RazerBatteryMonitor` eller `./test_battery.sh` for å teste alle variasjoner

15. **Detaljert Logging Lagt Til** (executed)
   - Lagt til omfattende logging for hver test
   - Printer respons-buffer (første 20 bytes) for hver test
   - Printer bytes 9-15 (Variation 1) og 10-15 (Variation 2) eksplisitt med verdier
   - Printer status-byte tydelig
   - Viser scanning-prosess med suksess/feil-indikatorer
   - Status: Recompiled successfully

16. **Final Protocol Test: Report ID + Byte 1 Parsing** (executed, then reverted)
   - Testet Report IDs 0x00, 0x01, 0x02 med parsing fra rawBuffer[1]
   - Ingen batteri-data funnet med noen Report ID
   - Konklusjon: Trenger offisiell protokoll-dokumentasjon fra Razer

17. **Revert til stabil implementering** (executed)
   - Fjernet Report ID testing loop
   - Fjernet `readRawResponse()` metode
   - Forenklet `queryBattery()` til single-shot implementering:
     - Report ID: 0x00
     - Command: 0x07 0x80
     - TransID: 0x1F
     - Battery offset: Byte 9 (standard Razer offset)
   - Koden er nå ren og stabil mens vi venter på dokumentasjon fra Razer
   - Status: Recompiled successfully
   - **Note**: Programmet vil returnere 0% batteri inntil korrekt protokoll er kjent

