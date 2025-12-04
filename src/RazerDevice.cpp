#include "RazerDevice.hpp"
#include <cstring>
#include <iostream>
#include <unistd.h>

RazerDevice::RazerDevice() : usbInterface_(nullptr), interfaceService_(0) {
}

RazerDevice::~RazerDevice() {
    disconnect();
}

bool RazerDevice::findInterface2(io_service_t device) {
    // Create iterator for device's interfaces
    io_iterator_t interfaceIterator;
    IOUSBFindInterfaceRequest request;
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    // We need to open the device first to iterate interfaces
    IOCFPlugInInterface** plugInInterface = nullptr;
    SInt32 score;
    kern_return_t kr = IOCreatePlugInInterfaceForService(device, kIOUSBDeviceUserClientTypeID,
                                                          kIOCFPlugInInterfaceID, &plugInInterface, &score);
    if (kr != KERN_SUCCESS || plugInInterface == nullptr) {
        std::cerr << "Failed to create device plugin interface" << std::endl;
        return false;
    }
    
    IOUSBDeviceInterface** deviceInterface = nullptr;
    HRESULT hr = (*plugInInterface)->QueryInterface(plugInInterface,
                    CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                    (LPVOID*)&deviceInterface);
    (*plugInInterface)->Release(plugInInterface);
    
    if (hr != S_OK || deviceInterface == nullptr) {
        std::cerr << "Failed to get device interface" << std::endl;
        return false;
    }
    
    // Open device to iterate interfaces
    kr = (*deviceInterface)->USBDeviceOpen(deviceInterface);
    if (kr != kIOReturnSuccess) {
        // Device might already be open by system, try to proceed anyway
        std::cout << "Note: Device already open by system (0x" << std::hex << kr << std::dec << ")" << std::endl;
    }
    
    // Create interface iterator
    kr = (*deviceInterface)->CreateInterfaceIterator(deviceInterface, &request, &interfaceIterator);
    if (kr != kIOReturnSuccess) {
        std::cerr << "Failed to create interface iterator" << std::endl;
        (*deviceInterface)->USBDeviceClose(deviceInterface);
        (*deviceInterface)->Release(deviceInterface);
        return false;
    }
    
    // Iterate through interfaces to find Interface 2
    io_service_t usbInterfaceRef;
    bool found = false;
    
    while ((usbInterfaceRef = IOIteratorNext(interfaceIterator)) != 0) {
        IOCFPlugInInterface** interfacePlugIn = nullptr;
        SInt32 interfaceScore;
        
        kr = IOCreatePlugInInterfaceForService(usbInterfaceRef, kIOUSBInterfaceUserClientTypeID,
                                                kIOCFPlugInInterfaceID, &interfacePlugIn, &interfaceScore);
        
        if (kr == KERN_SUCCESS && interfacePlugIn != nullptr) {
            IOUSBInterfaceInterface** interface = nullptr;
            hr = (*interfacePlugIn)->QueryInterface(interfacePlugIn,
                        CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                        (LPVOID*)&interface);
            (*interfacePlugIn)->Release(interfacePlugIn);
            
            if (hr == S_OK && interface != nullptr) {
                UInt8 interfaceNumber;
                (*interface)->GetInterfaceNumber(interface, &interfaceNumber);
                
                std::cout << "Found interface " << (int)interfaceNumber << std::endl;
                
                if (interfaceNumber == TARGET_INTERFACE) {
                    std::cout << "Target Interface 2 found!" << std::endl;
                    
                    // Open this interface
                    kr = (*interface)->USBInterfaceOpen(interface);
                    if (kr == kIOReturnSuccess) {
                        usbInterface_ = interface;
                        interfaceService_ = usbInterfaceRef;
                        found = true;
                        std::cout << "Interface 2 opened successfully!" << std::endl;
                    } else if (kr == kIOReturnExclusiveAccess) {
                        std::cout << "Interface 2 has exclusive access, trying without open..." << std::endl;
                        // Some operations might work without opening
                        usbInterface_ = interface;
                        interfaceService_ = usbInterfaceRef;
                        found = true;
                    } else {
                        std::cerr << "Failed to open Interface 2: 0x" << std::hex << kr << std::dec << std::endl;
                        (*interface)->Release(interface);
                    }
                    break;
                } else {
                    (*interface)->Release(interface);
                }
            }
        }
        
        if (!found) {
            IOObjectRelease(usbInterfaceRef);
        }
    }
    
    IOObjectRelease(interfaceIterator);
    
    // Close device (we'll use the interface directly)
    (*deviceInterface)->USBDeviceClose(deviceInterface);
    (*deviceInterface)->Release(deviceInterface);
    
    return found;
}

bool RazerDevice::connect() {
    if (usbInterface_ != nullptr) {
        return true; // Already connected
    }
    
    std::cout << "Searching for Razer Viper V2 Pro (VID: 0x" << std::hex << VENDOR_ID 
              << ", PID: 0x" << PRODUCT_ID << std::dec << ")..." << std::endl;
    
    // Create matching dictionary for USB devices
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (matchingDict == nullptr) {
        std::cerr << "Failed to create matching dictionary" << std::endl;
        return false;
    }
    
    // Add VID/PID to matching dictionary
    int vid = VENDOR_ID;
    int pid = PRODUCT_ID;
    CFNumberRef vidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef pidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), vidRef);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), pidRef);
    CFRelease(vidRef);
    CFRelease(pidRef);
    
    // Find the device
    io_iterator_t iterator;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to get matching services: " << kr << std::endl;
        return false;
    }
    
    // Get first matching device
    io_service_t deviceService = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    
    if (deviceService == 0) {
        std::cerr << "Device not found" << std::endl;
        return false;
    }
    
    std::cout << "Found device, searching for Interface 2..." << std::endl;
    
    // Find and open Interface 2
    bool success = findInterface2(deviceService);
    IOObjectRelease(deviceService);
    
    if (success) {
        // Initialize device: Set to Driver Mode (0x03) before querying
        std::cout << "\nInitializing device to Driver Mode..." << std::endl;
        if (!setDeviceMode(0x03, 0x00)) {
            std::cerr << "Warning: Failed to set Driver Mode (device may still work)" << std::endl;
        }
    }
    
    return success;
}

void RazerDevice::disconnect() {
    if (usbInterface_ != nullptr) {
        (*usbInterface_)->USBInterfaceClose(usbInterface_);
        (*usbInterface_)->Release(usbInterface_);
        usbInterface_ = nullptr;
    }
    if (interfaceService_ != 0) {
        IOObjectRelease(interfaceService_);
        interfaceService_ = 0;
    }
}

bool RazerDevice::setDeviceMode(uint8_t mode, uint8_t param) {
    if (usbInterface_ == nullptr) {
        return false;
    }
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "INITIALIZATION: Setting Device Mode" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Mode: 0x" << std::hex << (int)mode << std::dec;
    if (mode == 0x00) std::cout << " (Normal Mode)";
    else if (mode == 0x03) std::cout << " (Driver Mode)";
    std::cout << std::endl;
    std::cout << "Param: 0x" << std::hex << (int)param << std::dec << std::endl;
    
    // Build the 90-byte report for Set Device Mode
    // Command Class: 0x00, Command ID: 0x04, Data Size: 0x02
    uint8_t report[REPORT_SIZE];
    std::memset(report, 0, REPORT_SIZE);
    
    report[0] = 0x00;   // Status: New Command
    report[1] = 0x1F;   // Transaction ID: Wireless (consistent with Viper V2 Pro)
    report[2] = 0x00;   // Remaining Packets (high)
    report[3] = 0x00;   // Remaining Packets (low)
    report[4] = 0x00;   // Protocol Type
    report[5] = 0x02;   // Data Size
    report[6] = 0x00;   // Command Class: Device
    report[7] = 0x04;   // Command ID: Set Mode
    report[8] = mode;   // args[0]: Mode (0x03 = Driver Mode)
    report[9] = param;  // args[1]: Param (0x00)
    
    // Calculate checksum
    calculateChecksum(report);
    
    std::cout << "Report (first 12 bytes): ";
    for (size_t i = 0; i < 12; ++i) {
        std::printf("%02X ", report[i]);
    }
    std::cout << std::endl;
    
    // Send the report
    if (!sendReport(report)) {
        std::cerr << "Failed to send Set Device Mode command" << std::endl;
        return false;
    }
    
    // Wait for device to process
    usleep(100000);  // 100ms
    
    // Read response
    uint8_t response[REPORT_SIZE];
    std::memset(response, 0, REPORT_SIZE);
    
    if (!readResponse(response, REPORT_SIZE)) {
        std::cerr << "Failed to read Set Device Mode response" << std::endl;
        return false;
    }
    
    std::cout << "Response (first 12 bytes): ";
    for (size_t i = 0; i < 12; ++i) {
        std::printf("%02X ", response[i]);
    }
    std::cout << std::endl;
    
    uint8_t status = response[0];
    std::cout << "Response Status: 0x" << std::hex << (int)status << std::dec;
    if (status == 0x00) std::cout << " (Success)";
    else if (status == 0x02) std::cout << " (Busy)";
    else if (status == 0x03) std::cout << " (Failure)";
    std::cout << std::endl;
    
    std::cout << "============================================================\n" << std::endl;
    
    // Wait for mode switch to complete
    std::cout << "Waiting 500ms for device mode switch to complete..." << std::endl;
    usleep(500000);  // 500ms
    
    return (status == 0x00);
}

void RazerDevice::calculateChecksum(uint8_t* report) {
    uint8_t checksum = 0;
    // XOR bytes 2 through 87 (indices 2-87) - matches librazermacos
    for (size_t i = 2; i < 88; ++i) {
        checksum ^= report[i];
    }
    report[88] = checksum; // Store checksum in byte 88 (CRC position)
}

bool RazerDevice::sendReport(const uint8_t* report) {
    if (usbInterface_ == nullptr) {
        return false;
    }
    
    // USB Control Transfer - SET_REPORT via Interface
    // NOTE: wIndex = 0x00 for mice (per librazermacos), NOT the interface number!
    IOUSBDevRequest request;
    request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT;  // 0x21
    request.bRequest = HID_REQ_SET_REPORT;  // 0x09
    request.wValue = 0x0300;  // Feature Report, Report ID 0
    request.wIndex = 0x00;  // Protocol index for mice (librazermacos default)
    request.wLength = REPORT_SIZE;  // 90 bytes
    request.pData = (void*)report;
    
    IOReturn kr = (*usbInterface_)->ControlRequest(usbInterface_, 0, &request);
    
    if (kr != kIOReturnSuccess) {
        std::cerr << "Failed to send report: 0x" << std::hex << kr << std::dec << std::endl;
        return false;
    }
    
    return true;
}

bool RazerDevice::readResponse(uint8_t* buffer, size_t bufferSize) {
    if (usbInterface_ == nullptr || bufferSize < REPORT_SIZE) {
        return false;
    }
    
    // USB Control Transfer - GET_REPORT via Interface
    // NOTE: wIndex = 0x00 for mice (per librazermacos), NOT the interface number!
    IOUSBDevRequest request;
    request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN;  // 0xA1
    request.bRequest = HID_REQ_GET_REPORT;  // 0x01
    request.wValue = 0x0300;  // Feature Report, Report ID 0
    request.wIndex = 0x00;  // Protocol index for mice (librazermacos default)
    request.wLength = REPORT_SIZE;  // 90 bytes
    request.pData = buffer;
    
    IOReturn kr = (*usbInterface_)->ControlRequest(usbInterface_, 0, &request);
    
    if (kr != kIOReturnSuccess) {
        std::cerr << "Failed to read response: 0x" << std::hex << kr << std::dec << std::endl;
        return false;
    }
    
    return true;
}

bool RazerDevice::queryBattery(uint8_t& batteryPercent) {
    if (usbInterface_ == nullptr) {
        return false;
    }
    
    // ============================================================
    // BATTERY QUERY: Viper V2 Pro
    // Working combination: TransID 0x1F + Cmd 0x80
    // Accept Status 0x00 (Success) or 0x02 (Busy) with valid data
    // ============================================================
    
    // Build the 90-byte report
    uint8_t report[REPORT_SIZE];
    std::memset(report, 0, REPORT_SIZE);
    
    report[0] = 0x00;   // Status: New Command
    report[1] = 0x1F;   // Transaction ID: Wireless (works for Viper V2 Pro)
    report[2] = 0x00;   // Remaining Packets (high)
    report[3] = 0x00;   // Remaining Packets (low)
    report[4] = 0x00;   // Protocol Type
    report[5] = 0x02;   // Data Size
    report[6] = 0x07;   // Command Class: Power
    report[7] = 0x80;   // Command ID: Get Battery Level
    
    // Calculate checksum (XOR bytes 2-87, store in byte 88)
    calculateChecksum(report);
    
    // Send the report
    if (!sendReport(report)) {
        std::cerr << "Failed to send battery query" << std::endl;
        return false;
    }
    
    // Wait for device to process
    usleep(100000);  // 100ms
    
    // Read response
    uint8_t response[REPORT_SIZE];
    std::memset(response, 0, REPORT_SIZE);
    
    if (!readResponse(response, REPORT_SIZE)) {
        std::cerr << "Failed to read battery response" << std::endl;
        return false;
    }
    
    // Check response status
    uint8_t status = response[0];
    uint8_t rawBattery = response[9];  // Battery data at byte 9 (arguments[1])
    
    // Debug output
    std::cout << "Battery Query Response:" << std::endl;
    std::cout << "  Status: 0x" << std::hex << (int)status << std::dec;
    if (status == 0x00) std::cout << " (Success)";
    else if (status == 0x02) std::cout << " (Busy/Data Ready)";
    else if (status == 0x03) std::cout << " (Failure)";
    std::cout << std::endl;
    std::cout << "  Raw Battery (Byte 9): 0x" << std::hex << (int)rawBattery 
              << std::dec << " (" << (int)rawBattery << ")" << std::endl;
    
    // Accept Status 0x00 (Success) OR Status 0x02 (Busy) with valid data
    // Wireless devices often return data with Status 0x02
    if ((status == 0x00 || status == 0x02) && rawBattery > 0) {
        // Scale 0-255 to 0-100%
        batteryPercent = (rawBattery * 100) / 255;
        std::cout << "  Battery: " << (int)batteryPercent << "%" << std::endl;
        return true;
    }
    
    // If status is good but no data, still return what we have
    if (status == 0x00 || status == 0x02) {
        batteryPercent = (rawBattery * 100) / 255;
        std::cout << "  Battery: " << (int)batteryPercent << "% (may be charging or zero)" << std::endl;
        return true;
    }
    
    std::cerr << "Battery query failed with status: 0x" << std::hex << (int)status << std::dec << std::endl;
    batteryPercent = 0;
    return false;
}
