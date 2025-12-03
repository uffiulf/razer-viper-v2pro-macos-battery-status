#include <hidapi.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

int main() {
    if (hid_init() != 0) {
        std::cerr << "Failed to init hidapi" << std::endl;
        return 1;
    }

    // Try Interface 0 (mouse interface)
    std::cout << "Testing Interface 0 (Mouse Interface)..." << std::endl;

    hid_device* dev = hid_open(0x1532, 0x00A6, nullptr);
    if (!dev) {
        std::cerr << "Failed to open device" << std::endl;
        hid_exit();
        return 1;
    }

    std::cout << "Device opened (using default interface)" << std::endl;

    // Construct battery query using exact OpenRazer format
    uint8_t report[91];  // 1 (report ID) + 90 (data)
    memset(report, 0, 91);

    report[0] = 0x00;  // Report ID
    report[1] = 0x00;  // Status (0x00 for host->device)
    report[2] = 0x1F;  // Transaction ID
    report[3] = 0x00;  // Reserved
    report[4] = 0x00;  // Reserved
    report[5] = 0x00;  // Reserved
    report[6] = 0x02;  // Data size
    report[7] = 0x00;  // Reserved
    report[8] = 0x07;  // Command class
    report[9] = 0x80;  // Command ID
    // report[10-89] = 0x00 (parameters/arguments)

    // Calculate CRC (XOR of bytes 3-89, stored at byte 90)
    uint8_t crc = 0;
    for (int i = 3; i <= 89; ++i) {
        crc ^= report[i];
    }
    report[90] = crc;

    std::cout << "Sending query via feature report..." << std::endl;
    int result = hid_send_feature_report(dev, report, 91);
    std::cout << "Send result: " << result << " bytes" << std::endl;

    usleep(100000);  // 100ms

    // Read response
    memset(report, 0, 91);
    report[0] = 0x00;
    result = hid_get_feature_report(dev, report, 91);

    if (result > 0) {
        std::cout << "Read " << result << " bytes:" << std::endl;

        // Print first 20 bytes
        for (int i = 0; i < 20 && i < result; ++i) {
            printf("%02X ", report[i]);
        }
        std::cout << std::endl;

        std::cout << "Battery at byte 11 (arguments[1]): 0x" << std::hex
                  << (int)report[11] << std::dec << " (" << (int)report[11] << ")" << std::endl;

        if (report[11] != 0) {
            int percentage = (report[11] * 100) / 255;
            std::cout << "*** BATTERY FOUND: " << percentage << "% ***" << std::endl;
        }
    } else {
        std::cout << "Read failed" << std::endl;
    }

    hid_close(dev);
    hid_exit();
    return 0;
}
