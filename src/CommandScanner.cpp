#include <hidapi.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <unistd.h>

// Command Class and ID pairs to test
struct CommandPair {
    uint8_t cmdClass;
    uint8_t cmdID;
    const char* description;
};

std::vector<CommandPair> commandsToTest = {
    // Class 0x00 - General/Device Info
    {0x00, 0x02, "Class 0x00, Cmd 0x02 - Device Info"},
    {0x00, 0x04, "Class 0x00, Cmd 0x04 - Serial"},
    {0x00, 0x16, "Class 0x00, Cmd 0x16 - Firmware"},
    {0x00, 0x80, "Class 0x00, Cmd 0x80 - Battery (alt class)"},
    {0x00, 0x82, "Class 0x00, Cmd 0x82 - Charging (alt class)"},

    // Class 0x02 - Wireless/Dongle specific
    {0x02, 0x80, "Class 0x02, Cmd 0x80 - Battery"},
    {0x02, 0x81, "Class 0x02, Cmd 0x81 - Battery State"},
    {0x02, 0x82, "Class 0x02, Cmd 0x82 - Charging"},
    {0x02, 0x07, "Class 0x02, Cmd 0x07 - Device Status"},

    // Class 0x03 - Power Management
    {0x03, 0x80, "Class 0x03, Cmd 0x80 - Battery"},
    {0x03, 0x81, "Class 0x03, Cmd 0x81 - Battery State"},
    {0x03, 0x00, "Class 0x03, Cmd 0x00 - Power Info"},

    // Class 0x07 - Standard (already tested, but include key ones)
    {0x07, 0x80, "Class 0x07, Cmd 0x80 - Battery Level"},
    {0x07, 0x81, "Class 0x07, Cmd 0x81 - Battery State"},
    {0x07, 0x82, "Class 0x07, Cmd 0x82 - Charging Status"},
    {0x07, 0x84, "Class 0x07, Cmd 0x84 - Battery (alt)"},

    // Class 0x0F - Extended features
    {0x0F, 0x80, "Class 0x0F, Cmd 0x80 - Battery"},
    {0x0F, 0x82, "Class 0x0F, Cmd 0x82 - Charging"},
};

void testCommand(hid_device* device, uint8_t cmdClass, uint8_t cmdID, const char* description) {
    std::cout << "\n=== Testing " << description << " ===" << std::endl;

    // Construct report
    uint8_t report[90];
    std::memset(report, 0, 90);

    // Step 1: NEW_REQUEST
    report[0] = 0x02;  // Status
    report[1] = 0x1F;  // Transaction ID
    report[6] = cmdClass;  // Command Class
    report[7] = cmdID;     // Command ID

    // Calculate checksum
    uint8_t checksum = 0;
    for (size_t i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum;

    // Send NEW_REQUEST
    uint8_t writeBuffer[91];
    writeBuffer[0] = 0x00;  // Report ID
    std::memcpy(writeBuffer + 1, report, 90);

    if (hid_send_feature_report(device, writeBuffer, 91) != 91) {
        std::cout << "Failed to send NEW_REQUEST" << std::endl;
        return;
    }

    usleep(200000);  // 200ms

    // Step 2: RETRIEVE
    report[0] = 0x00;  // Status
    checksum = 0;
    for (size_t i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum;

    std::memcpy(writeBuffer + 1, report, 90);

    if (hid_send_feature_report(device, writeBuffer, 91) != 91) {
        std::cout << "Failed to send RETRIEVE" << std::endl;
        return;
    }

    usleep(50000);  // 50ms

    // Read response
    uint8_t readBuffer[91];
    readBuffer[0] = 0x00;
    int result = hid_get_feature_report(device, readBuffer, 91);

    if (result < 0) {
        std::cout << "Failed to read response" << std::endl;
        return;
    }

    uint8_t* response = readBuffer + 1;

    std::cout << "Status: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)response[0] << std::dec << std::endl;

    // Check if any data bytes are non-zero
    bool hasData = false;
    for (size_t i = 8; i < 89; ++i) {
        if (response[i] != 0x00) {
            hasData = true;
            break;
        }
    }

    if (hasData) {
        std::cout << "*** DATA FOUND! ***" << std::endl;
        std::cout << "Full response:" << std::endl;
        for (size_t i = 0; i < 90; ++i) {
            std::printf("%02X ", response[i]);
            if ((i + 1) % 16 == 0) std::cout << std::endl;
        }
        std::cout << std::endl;

        // Show first 20 data bytes
        std::cout << "First 20 data bytes: ";
        for (size_t i = 8; i < 28 && i < 90; ++i) {
            std::printf("%02X ", response[i]);
        }
        std::cout << std::endl;
    } else {
        std::cout << "Empty response (all data bytes are 0x00)" << std::endl;
    }
}

int main() {
    if (hid_init() != 0) {
        std::cerr << "Failed to initialize hidapi" << std::endl;
        return 1;
    }

    // Find and open Interface 2
    struct hid_device_info* devs = hid_enumerate(0x1532, 0x00A6);
    struct hid_device_info* cur_dev = devs;

    std::string target_path;
    while (cur_dev) {
        if (cur_dev->interface_number == 2) {
            target_path = std::string(cur_dev->path);
            std::cout << "Found Interface 2: " << target_path << std::endl;
            break;
        }
        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);

    if (target_path.empty()) {
        std::cerr << "Interface 2 not found" << std::endl;
        hid_exit();
        return 1;
    }

    hid_device* device = hid_open_path(target_path.c_str());
    if (device == nullptr) {
        std::cerr << "Failed to open device" << std::endl;
        hid_exit();
        return 1;
    }

    std::cout << "Device opened successfully\n" << std::endl;

    // Test all commands
    for (const auto& cmd : commandsToTest) {
        testCommand(device, cmd.cmdClass, cmd.cmdID, cmd.description);
        usleep(100000);  // 100ms between tests
    }

    hid_close(device);
    hid_exit();

    return 0;
}
