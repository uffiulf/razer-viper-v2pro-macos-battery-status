#ifndef RAZER_DEVICE_HPP
#define RAZER_DEVICE_HPP

#include <cstdint>
#include <string>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

// Callback type for device change events
typedef void (*DeviceCallback)(void* context);

class RazerDevice {
public:
    RazerDevice();
    ~RazerDevice();
    
    bool connect();
    void disconnect();
    bool queryBattery(uint8_t& batteryPercent);
    bool queryChargingStatus(bool& isCharging);
    bool isConnected() const { return usbInterface_ != nullptr; }
    
    // Hotplug monitoring
    void startMonitoring(DeviceCallback callback, void* context);
    void stopMonitoring();

private:
    static constexpr uint16_t VENDOR_ID = 0x1532;
    static constexpr uint16_t PRODUCT_ID_DONGLE = 0x00A6;  // Wireless Dongle
    static constexpr uint16_t PRODUCT_ID_WIRED = 0x00A5;   // Wired Mouse (Charging)
    static constexpr size_t REPORT_SIZE = 90;
    static constexpr uint8_t TARGET_INTERFACE = 2;  // Interface 2 for control
    
    // USB HID Request types
    static constexpr uint8_t USB_TYPE_CLASS = 0x01 << 5;
    static constexpr uint8_t USB_RECIP_INTERFACE = 0x01;
    static constexpr uint8_t USB_DIR_OUT = 0x00;
    static constexpr uint8_t USB_DIR_IN = 0x80;
    static constexpr uint8_t HID_REQ_SET_REPORT = 0x09;
    static constexpr uint8_t HID_REQ_GET_REPORT = 0x01;
    
    IOUSBInterfaceInterface** usbInterface_;
    io_service_t interfaceService_;
    
    // Wired vs. Wireless detection
    bool isDongle_;  // true = Wireless (Dongle), false = Wired (Direct USB)
    std::string getDeviceName(io_service_t device);
    
    // IOKit notification members
    IONotificationPortRef notificationPort_;
    io_iterator_t addedIter_;
    io_iterator_t removedIter_;
    DeviceCallback callback_;
    void* callbackContext_;
    
    void calculateChecksum(uint8_t* report);
    bool sendReport(const uint8_t* report);
    bool readResponse(uint8_t* buffer, size_t bufferSize);
    bool findInterface2(io_service_t device);
    bool setDeviceMode(uint8_t mode, uint8_t param);
    void debugCharging();  // DEBUG: Test charging commands
    
    // Static callbacks for IOKit
    static void deviceAddedCallback(void* refCon, io_iterator_t iterator);
    static void deviceRemovedCallback(void* refCon, io_iterator_t iterator);
};

#endif // RAZER_DEVICE_HPP
