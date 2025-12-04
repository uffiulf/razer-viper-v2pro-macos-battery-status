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
    // XOR bytes 2 through 87 (indices 2-87) - matches librazermacos
    // This is 86 bytes: [2, 3, 4, ..., 87]
    for (size_t i = 2; i < 88; ++i) {
        checksum ^= report[i];
    }
    report[88] = checksum; // Store checksum in byte 88 (CRC position)
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
    readBuffer[0] = 0x00;
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
    
    // Clean, stable single-shot implementation
    // Protocol: Report ID 0x00, Command 0x07 0x80, TransID 0x1F
    // Awaiting official documentation from Razer for correct byte offset
    
    // Build the 90-byte report
    uint8_t report[REPORT_SIZE];
    std::memset(report, 0, REPORT_SIZE);
    
    report[0] = 0x00;  // Status
    report[1] = 0x1F;  // Transaction ID (wireless)
    report[5] = 0x02;  // Data size
    report[7] = 0x07;  // Command Class (Power)
    report[8] = 0x80;  // Command ID (Get Battery)
    
    // Calculate checksum (XOR bytes 2-88)
    calculateChecksum(report);
    
    // Send command
    if (!sendReport(report)) {
        std::cerr << "Failed to send battery query" << std::endl;
        return false;
    }
    
    // Wait for device response
    usleep(500000);  // 500ms
    
    // Read response
    uint8_t response[REPORT_SIZE];
    if (!readResponse(response, REPORT_SIZE)) {
        std::cerr << "Failed to read battery response" << std::endl;
        return false;
    }
    
    // Check status byte
    if (response[0] != 0x00) {
        std::cerr << "Battery query failed with status: 0x" << std::hex << (int)response[0] << std::dec << std::endl;
        return false;
    }
    
    // Parse battery from byte 9 (standard Razer offset)
    // Note: This may return 0 until we have the correct protocol from Razer
    uint8_t rawBattery = response[9];
    batteryPercent = (rawBattery * 100) / 255;
    
    return true;
}

