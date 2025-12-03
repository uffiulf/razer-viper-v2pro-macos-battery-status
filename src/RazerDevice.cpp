#include "RazerDevice.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

RazerDevice::RazerDevice() : device_(nullptr) {
    if (hid_init() != 0) {
        std::cerr << "Failed to initialize hidapi" << std::endl;
    }
}

RazerDevice::~RazerDevice() {
    disconnect();
    hid_exit();
}

bool RazerDevice::connect() {
    if (device_ != nullptr) {
        return true; // Already connected
    }
    
    // Enumerate all HID devices matching VID/PID to find vendor-specific interface
    std::cout << "Enumerating HID devices (VID: 0x" << std::hex << VENDOR_ID 
              << ", PID: 0x" << PRODUCT_ID << std::dec << ")..." << std::endl;
    
    struct hid_device_info* devs = hid_enumerate(VENDOR_ID, PRODUCT_ID);
    struct hid_device_info* cur_dev = devs;
    
    // STRICT: Only connect to Interface 2 (control interface for battery queries)
    std::string target_path;
    int target_interface = -1;
    
    while (cur_dev) {
        // Only accept Interface 2 - skip all others
        if (cur_dev->interface_number == 2) {
            target_path = std::string(cur_dev->path);
            target_interface = cur_dev->interface_number;
            std::cout << "Found Interface 2 (control interface):" << std::endl;
            std::cout << "  Interface Number: " << cur_dev->interface_number << std::endl;
            std::cout << "  Usage Page: 0x" << std::hex << cur_dev->usage_page << std::dec << std::endl;
            std::cout << "  Path: " << cur_dev->path << std::endl;
            break; // Found Interface 2, stop searching
        }
        
        cur_dev = cur_dev->next;
    }
    
    // Free enumeration list
    hid_free_enumeration(devs);
    
    if (target_path.empty()) {
        std::cerr << "Failed to find Interface 2 (required for battery queries)" << std::endl;
        return false;
    }
    
    // Connect to Interface 2 only
    std::cout << "\nConnecting to Interface #" << target_interface 
              << " at path: " << target_path << std::endl;
    device_ = hid_open_path(target_path.c_str());
    
    if (device_ == nullptr) {
        std::cerr << "Failed to open Razer Viper V2 Pro on Interface 2" << std::endl;
        const wchar_t* error = hid_error(nullptr);
        if (error) {
            std::wcerr << "HID Error: " << error << std::endl;
        }
        return false;
    }
    
    std::cout << "Device opened successfully on Interface #" << target_interface << "!" << std::endl;
    return true;
}

void RazerDevice::disconnect() {
    if (device_ != nullptr) {
        hid_close(device_);
        device_ = nullptr;
    }
}

void RazerDevice::calculateChecksum(uint8_t* report) {
    uint8_t checksum = 0;
    // XOR bytes 2 through 88 (indices 2-88)
    for (size_t i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum; // Store checksum in byte 89
}

bool RazerDevice::sendReport(const uint8_t* report) {
    if (device_ == nullptr) {
        return false;
    }
    
    // Prepend 0x00 as Report ID for feature report (91-byte buffer)
    uint8_t writeBuffer[WRITE_BUFFER_SIZE];
    writeBuffer[0] = 0x00;
    std::memcpy(writeBuffer + 1, report, REPORT_SIZE);
    
    // Use hid_send_feature_report for wireless dongle compatibility
    int result = hid_send_feature_report(device_, writeBuffer, WRITE_BUFFER_SIZE);
    return result == WRITE_BUFFER_SIZE;
}

bool RazerDevice::readResponse(uint8_t* buffer, size_t bufferSize) {
    if (device_ == nullptr) {
        return false;
    }
    
    // Read feature report (Report ID will be first byte)
    uint8_t readBuffer[WRITE_BUFFER_SIZE];
    readBuffer[0] = 0x00; // Set Report ID for feature report request
    int result = hid_get_feature_report(device_, readBuffer, WRITE_BUFFER_SIZE);
    
    if (result < 0) {
        return false;
    }
    
    // Copy response, skipping Report ID (first byte)
    size_t copySize = (result > 1) ? result - 1 : 0;
    copySize = (copySize < bufferSize) ? copySize : bufferSize;
    std::memcpy(buffer, readBuffer + 1, copySize);
    
    return copySize >= REPORT_SIZE;
}

bool RazerDevice::queryBattery(uint8_t& batteryPercent) {
    if (device_ == nullptr) {
        return false;
    }
    
    // Try multiple protocol variations and command IDs systematically
    // Based on research: QuickTest uses different byte offsets than our current implementation
    
    // Protocol variations to try:
    // Variation 1: Current (Byte 5=Data Size, Byte 7=Class, Byte 8=Command)
    // Variation 2: QuickTest style (Byte 6=Data Size, Byte 8=Class, Byte 9=Command)
    // Commands to try: 0x80, 0x82, 0x83
    
    const int commands[] = {0x80, 0x82, 0x83};
    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    
    for (int cmdIdx = 0; cmdIdx < numCommands; ++cmdIdx) {
        uint8_t cmdId = commands[cmdIdx];
        
        // Try Variation 1: Current structure
        std::cout << "\n=== Trying Command 0x" << std::hex << (int)cmdId << std::dec 
                  << " with Variation 1 (Byte 5=Data Size) ===" << std::endl;
        
        uint8_t report[REPORT_SIZE];
        std::memset(report, 0, REPORT_SIZE);
        
        report[0] = 0x00;  // Status
        report[1] = 0x1F;  // Transaction ID
        report[5] = 0x02;  // Data size
        report[7] = 0x07;  // Command Class
        report[8] = cmdId; // Command ID
        
        calculateChecksum(report);
        
        // Wake-up delay
        usleep(200000);  // 200ms
        
        if (!sendReport(report)) {
            std::cerr << "Failed to send query (Variation 1, Cmd 0x" << std::hex << (int)cmdId << std::dec << ")" << std::endl;
            continue;
        }
        
        // Wait for wireless device (longer wait)
        usleep(1000000);  // 1000ms (1 second)
        
        uint8_t response[REPORT_SIZE];
        if (!readResponse(response, REPORT_SIZE)) {
            std::cerr << "Failed to read response (Variation 1, Cmd 0x" << std::hex << (int)cmdId << std::dec << ")" << std::endl;
            continue;
        }
        
        // DETAILED LOGGING: Print response buffer
        std::cout << "  Response Status: 0x" << std::hex << (int)response[0] << std::dec << std::endl;
        std::cout << "  Response buffer (first 20 bytes): ";
        for (size_t i = 0; i < 20; ++i) {
            std::printf("%02X ", response[i]);
        }
        std::cout << std::endl;
        std::cout << "  Bytes 9-15 (data area): ";
        for (size_t i = 9; i <= 15; ++i) {
            std::printf("Byte[%zu]=0x%02X(%d) ", i, response[i], response[i]);
        }
        std::cout << std::endl;
        
        if (response[0] != 0x00) {
            std::cout << "  ⚠️  Status not 0x00: 0x" << std::hex << (int)response[0] << std::dec << " - skipping" << std::endl;
            continue;
        }
        
        // Scan for battery data in bytes 9-15
        std::cout << "  Scanning bytes 9-15 for non-zero data (excluding command echo 0x" << std::hex << (int)cmdId << std::dec << ")..." << std::endl;
        bool foundData = false;
        for (size_t i = 9; i <= 15; ++i) {
            if (response[i] != 0x00 && response[i] != cmdId) {  // Ignore command echo
                uint8_t raw = response[i];
                if (raw > 0 && raw <= 255) {
                    batteryPercent = (raw * 100) / 255;
                    std::cout << "  ✅ *** SUCCESS: Found battery data at byte " << i 
                              << ": 0x" << std::hex << (int)raw << std::dec 
                              << " = " << (int)batteryPercent << "% ***" << std::endl;
                    return true;
                }
            }
        }
        if (!foundData) {
            std::cout << "  ❌ No valid battery data found in bytes 9-15" << std::endl;
        }
        
        // Try Variation 2: QuickTest style structure
        std::cout << "\n=== Trying Command 0x" << std::hex << (int)cmdId << std::dec 
                  << " with Variation 2 (Byte 6=Data Size, QuickTest style) ===" << std::endl;
        
        std::memset(report, 0, REPORT_SIZE);
        report[0] = 0x00;  // Status
        report[1] = 0x1F;  // Transaction ID
        report[6] = 0x02;  // Data size (different offset)
        report[8] = 0x07;  // Command Class (different offset)
        report[9] = cmdId; // Command ID (different offset)
        
        // Calculate checksum for Variation 2 (XOR bytes 2-88)
        calculateChecksum(report);
        
        usleep(200000);  // 200ms
        
        if (!sendReport(report)) {
            std::cerr << "Failed to send query (Variation 2, Cmd 0x" << std::hex << (int)cmdId << std::dec << ")" << std::endl;
            continue;
        }
        
        usleep(1000000);  // 1000ms
        
        if (!readResponse(response, REPORT_SIZE)) {
            std::cerr << "Failed to read response (Variation 2, Cmd 0x" << std::hex << (int)cmdId << std::dec << ")" << std::endl;
            continue;
        }
        
        // DETAILED LOGGING: Print response buffer
        std::cout << "  Response Status: 0x" << std::hex << (int)response[0] << std::dec << std::endl;
        std::cout << "  Response buffer (first 20 bytes): ";
        for (size_t i = 0; i < 20; ++i) {
            std::printf("%02X ", response[i]);
        }
        std::cout << std::endl;
        std::cout << "  Bytes 10-15 (data area): ";
        for (size_t i = 10; i <= 15; ++i) {
            std::printf("Byte[%zu]=0x%02X(%d) ", i, response[i], response[i]);
        }
        std::cout << std::endl;
        
        if (response[0] != 0x00) {
            std::cout << "  ⚠️  Status not 0x00: 0x" << std::hex << (int)response[0] << std::dec << " - skipping" << std::endl;
            continue;
        }
        
        // Scan for battery data (check byte 11 as per QuickTest)
        std::cout << "  Scanning bytes 10-15 for non-zero data (excluding command echo 0x" << std::hex << (int)cmdId << std::dec << ")..." << std::endl;
        bool foundData2 = false;
        for (size_t i = 10; i <= 15; ++i) {
            if (response[i] != 0x00 && response[i] != cmdId) {
                uint8_t raw = response[i];
                if (raw > 0 && raw <= 255) {
                    batteryPercent = (raw * 100) / 255;
                    std::cout << "  ✅ *** SUCCESS: Found battery data at byte " << i 
                              << ": 0x" << std::hex << (int)raw << std::dec 
                              << " = " << (int)batteryPercent << "% ***" << std::endl;
                    return true;
                }
            }
        }
        if (!foundData2) {
            std::cout << "  ❌ No valid battery data found in bytes 10-15" << std::endl;
        }
    }
    
    std::cerr << "\n*** FAILED: No battery data found after trying all variations ***" << std::endl;
    return false;
}

