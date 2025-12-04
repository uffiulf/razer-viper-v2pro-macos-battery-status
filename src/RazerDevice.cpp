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
    IOUSBDevRequest request;
    request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT;  // 0x21
    request.bRequest = HID_REQ_SET_REPORT;  // 0x09
    request.wValue = 0x0300;  // Feature Report, Report ID 0
    request.wIndex = TARGET_INTERFACE;  // Interface 2
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
    IOUSBDevRequest request;
    request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN;  // 0xA1
    request.bRequest = HID_REQ_GET_REPORT;  // 0x01
    request.wValue = 0x0300;  // Feature Report, Report ID 0
    request.wIndex = TARGET_INTERFACE;  // Interface 2
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
    // MATRIX TEST: Systematic Protocol Discovery
    // ============================================================
    // Test all combinations of Transaction IDs and Commands
    // to find the correct protocol for Viper V2 Pro (0x00A6)
    // ============================================================
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "MATRIX TEST: Protocol Discovery for Viper V2 Pro" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Testing Transaction IDs: 0xFF (Wired), 0x1F (Wireless), 0x3F (Pro)" << std::endl;
    std::cout << "Testing Commands: 0x80 (Battery), 0x82 (Charging Status)" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // Transaction IDs to test
    const uint8_t transactionIds[] = {0xFF, 0x1F, 0x3F};
    const char* transIdNames[] = {"0xFF (Wired/Default)", "0x1F (Wireless/Newer)", "0x3F (Pro Models)"};
    const int numTransIds = 3;
    
    // Commands to test
    const uint8_t commands[] = {0x80, 0x82};
    const char* cmdNames[] = {"0x80 (Get Battery)", "0x82 (Get Charging Status)"};
    const int numCmds = 2;
    
    int testNumber = 0;
    
    // Matrix test: all combinations
    for (int t = 0; t < numTransIds; ++t) {
        for (int c = 0; c < numCmds; ++c) {
            testNumber++;
            uint8_t transId = transactionIds[t];
            uint8_t cmdId = commands[c];
            
            std::cout << "------------------------------------------------------------" << std::endl;
            std::cout << "TEST " << testNumber << "/" << (numTransIds * numCmds) << ": "
                      << "TransID " << transIdNames[t] << " + Cmd " << cmdNames[c] << std::endl;
            std::cout << "------------------------------------------------------------" << std::endl;
            
            // Build the report
            uint8_t report[REPORT_SIZE];
            std::memset(report, 0, REPORT_SIZE);
            
            report[0] = 0x00;   // Status: New Command
            report[1] = transId; // Transaction ID (varies)
            report[2] = 0x00;   // Remaining Packets (high)
            report[3] = 0x00;   // Remaining Packets (low)
            report[4] = 0x00;   // Protocol Type
            report[5] = 0x02;   // Data Size
            report[6] = 0x07;   // Command Class: Power
            report[7] = cmdId;  // Command ID (varies)
            
            // Calculate checksum
            calculateChecksum(report);
            
            // Send the report
            if (!sendReport(report)) {
                std::cout << "  [SKIP] Failed to send report" << std::endl;
                continue;
            }
            
            // Wait for device to process
            usleep(100000);  // 100ms
            
            // Read response
            uint8_t response[REPORT_SIZE];
            std::memset(response, 0, REPORT_SIZE);
            
            if (!readResponse(response, REPORT_SIZE)) {
                std::cout << "  [SKIP] Failed to read response" << std::endl;
                continue;
            }
            
            // Print response summary
            std::cout << "  Response (first 12 bytes): ";
            for (size_t i = 0; i < 12; ++i) {
                std::printf("%02X ", response[i]);
            }
            std::cout << std::endl;
            
            // Check key values
            uint8_t status = response[0];
            uint8_t byte8 = response[8];   // arguments[0]
            uint8_t byte9 = response[9];   // arguments[1] - expected battery location
            uint8_t byte10 = response[10]; // arguments[2]
            
            std::cout << "  Status: 0x" << std::hex << (int)status << std::dec;
            if (status == 0x00) std::cout << " (Success)";
            else if (status == 0x02) std::cout << " (Busy)";
            else if (status == 0x03) std::cout << " (Failure)";
            std::cout << std::endl;
            
            std::cout << "  Byte 8 (args[0]): 0x" << std::hex << (int)byte8 << std::dec 
                      << " (" << (int)byte8 << ")" << std::endl;
            std::cout << "  Byte 9 (args[1]): 0x" << std::hex << (int)byte9 << std::dec 
                      << " (" << (int)byte9 << ")" << std::endl;
            std::cout << "  Byte 10 (args[2]): 0x" << std::hex << (int)byte10 << std::dec 
                      << " (" << (int)byte10 << ")" << std::endl;
            
            // Check for SUCCESS: any non-zero value in data bytes
            bool foundData = false;
            int dataLocation = -1;
            uint8_t dataValue = 0;
            
            // Check bytes 8, 9, 10 for potential battery data
            for (int i = 8; i <= 10; ++i) {
                if (response[i] != 0x00) {
                    foundData = true;
                    dataLocation = i;
                    dataValue = response[i];
                    break;
                }
            }
            
            if (foundData && status == 0x00) {
                std::cout << "\n  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
                std::cout << "  >>> POTENTIAL SUCCESS DETECTED! <<<" << std::endl;
                std::cout << "  >>> TransID: 0x" << std::hex << (int)transId << std::dec << std::endl;
                std::cout << "  >>> Command: 0x" << std::hex << (int)cmdId << std::dec << std::endl;
                std::cout << "  >>> Data at Byte " << dataLocation << ": 0x" << std::hex << (int)dataValue 
                          << std::dec << " (" << (int)dataValue << ")" << std::endl;
                std::cout << "  >>> Battery: " << ((int)dataValue * 100 / 255) << "%" << std::endl;
                std::cout << "  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n" << std::endl;
                
                // Return this as the battery value
                batteryPercent = (dataValue * 100) / 255;
                return true;
            } else {
                std::cout << "  [NO DATA] Empty response for this combination" << std::endl;
            }
            
            std::cout << std::endl;
        }
    }
    
    std::cout << "============================================================" << std::endl;
    std::cout << "MATRIX TEST COMPLETE: No valid battery data found" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    batteryPercent = 0;
    return false;
}
