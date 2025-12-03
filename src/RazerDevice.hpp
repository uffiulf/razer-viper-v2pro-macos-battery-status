#ifndef RAZER_DEVICE_HPP
#define RAZER_DEVICE_HPP

#include <cstdint>
#include <hidapi.h>

class RazerDevice {
public:
    RazerDevice();
    ~RazerDevice();
    
    bool connect();
    void disconnect();
    bool queryBattery(uint8_t& batteryPercent);
    bool isConnected() const { return device_ != nullptr; }

private:
    static constexpr uint16_t VENDOR_ID = 0x1532;
    static constexpr uint16_t PRODUCT_ID = 0x00A6;
    static constexpr size_t REPORT_SIZE = 90;
    static constexpr size_t WRITE_BUFFER_SIZE = 91; // Report + Report ID
    
    hid_device* device_;
    
    void calculateChecksum(uint8_t* report);
    bool sendReport(const uint8_t* report);
    bool readResponse(uint8_t* buffer, size_t bufferSize);
};

#endif // RAZER_DEVICE_HPP

