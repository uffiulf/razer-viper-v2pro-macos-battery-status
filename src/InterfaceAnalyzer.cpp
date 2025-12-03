#include <hidapi.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <unistd.h>

void analyzeInterface(const char* path, int interfaceNum) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Analyzing Interface " << interfaceNum << std::endl;
    std::cout << "Path: " << path << std::endl;
    std::cout << "========================================" << std::endl;

    hid_device* device = hid_open_path(path);
    if (!device) {
        std::cerr << "Failed to open interface" << std::endl;
        return;
    }

    // Get device info
    wchar_t wstr[256];

    if (hid_get_manufacturer_string(device, wstr, 256) == 0) {
        std::wcout << L"Manufacturer: " << wstr << std::endl;
    }

    if (hid_get_product_string(device, wstr, 256) == 0) {
        std::wcout << L"Product: " << wstr << std::endl;
    }

    if (hid_get_serial_number_string(device, wstr, 256) == 0) {
        std::wcout << L"Serial: " << wstr << std::endl;
    }

    std::cout << "\n--- Test 1: Try reading feature report (no command) ---" << std::endl;
    uint8_t readBuf[91];
    memset(readBuf, 0, 91);
    readBuf[0] = 0x00;
    int result = hid_get_feature_report(device, readBuf, 91);
    if (result > 0) {
        std::cout << "Read " << result << " bytes:" << std::endl;
        bool hasData = false;
        for (int i = 1; i < result && i < 91; ++i) {
            if (readBuf[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            std::cout << "*** DATA FOUND (unsolicited) ***" << std::endl;
            for (int i = 0; i < result && i < 91; ++i) {
                std::printf("%02X ", readBuf[i]);
                if ((i + 1) % 16 == 0) std::cout << std::endl;
            }
            std::cout << std::endl;
        } else {
            std::cout << "Empty data" << std::endl;
        }
    } else {
        std::cout << "Read failed or returned 0 bytes" << std::endl;
    }

    std::cout << "\n--- Test 2: Try standard battery query (Class 0x07, Cmd 0x80) ---" << std::endl;
    uint8_t report[90];
    memset(report, 0, 90);
    report[0] = 0x02;  // NEW_REQUEST
    report[1] = 0x1F;  // Transaction ID
    report[6] = 0x07;  // Command Class
    report[7] = 0x80;  // Command ID - Battery

    uint8_t checksum = 0;
    for (int i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum;

    uint8_t writeBuf[91];
    writeBuf[0] = 0x00;
    memcpy(writeBuf + 1, report, 90);

    result = hid_send_feature_report(device, writeBuf, 91);
    std::cout << "Send result: " << result << " bytes" << std::endl;

    usleep(200000);

    report[0] = 0x00;  // RETRIEVE
    checksum = 0;
    for (int i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum;
    memcpy(writeBuf + 1, report, 90);

    hid_send_feature_report(device, writeBuf, 91);
    usleep(50000);

    memset(readBuf, 0, 91);
    readBuf[0] = 0x00;
    result = hid_get_feature_report(device, readBuf, 91);

    if (result > 0) {
        std::cout << "Response Status: 0x" << std::hex << std::setfill('0')
                  << std::setw(2) << (int)readBuf[1] << std::dec << std::endl;

        bool hasData = false;
        for (int i = 9; i < result && i < 91; ++i) {
            if (readBuf[i] != 0x00) {
                hasData = true;
                break;
            }
        }

        if (hasData) {
            std::cout << "*** DATA FOUND ***" << std::endl;
            for (int i = 0; i < result && i < 91; ++i) {
                std::printf("%02X ", readBuf[i]);
                if ((i + 1) % 16 == 0) std::cout << std::endl;
            }
            std::cout << std::endl;
        } else {
            std::cout << "Empty response" << std::endl;
        }
    }

    std::cout << "\n--- Test 3: Try reading without Double Tap (Status 0x00 only) ---" << std::endl;
    memset(report, 0, 90);
    report[0] = 0x00;  // Standard query
    report[1] = 0x1F;
    report[6] = 0x07;
    report[7] = 0x80;

    checksum = 0;
    for (int i = 2; i <= 88; ++i) {
        checksum ^= report[i];
    }
    report[89] = checksum;

    memcpy(writeBuf + 1, report, 90);
    hid_send_feature_report(device, writeBuf, 91);
    usleep(100000);

    memset(readBuf, 0, 91);
    readBuf[0] = 0x00;
    result = hid_get_feature_report(device, readBuf, 91);

    if (result > 0) {
        bool hasData = false;
        for (int i = 1; i < result && i < 91; ++i) {
            if (readBuf[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            std::cout << "*** DATA FOUND ***" << std::endl;
            for (int i = 0; i < result && i < 91; ++i) {
                std::printf("%02X ", readBuf[i]);
                if ((i + 1) % 16 == 0) std::cout << std::endl;
            }
            std::cout << std::endl;
        } else {
            std::cout << "Empty response" << std::endl;
        }
    }

    std::cout << "\n--- Test 4: Try input report (hid_read with timeout) ---" << std::endl;
    memset(readBuf, 0, 91);
    result = hid_read_timeout(device, readBuf, 91, 500);  // 500ms timeout
    if (result > 0) {
        std::cout << "Read " << result << " bytes via input report:" << std::endl;
        bool hasData = false;
        for (int i = 0; i < result; ++i) {
            if (readBuf[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            std::cout << "*** DATA FOUND (input report) ***" << std::endl;
            for (int i = 0; i < result; ++i) {
                std::printf("%02X ", readBuf[i]);
                if ((i + 1) % 16 == 0) std::cout << std::endl;
            }
            std::cout << std::endl;
        } else {
            std::cout << "Empty data" << std::endl;
        }
    } else if (result == 0) {
        std::cout << "Timeout - no input report available" << std::endl;
    } else {
        std::cout << "Read failed" << std::endl;
    }

    hid_close(device);
}

int main() {
    if (hid_init() != 0) {
        std::cerr << "Failed to initialize hidapi" << std::endl;
        return 1;
    }

    std::cout << "Enumerating ALL Razer Viper V2 Pro interfaces..." << std::endl;

    struct hid_device_info* devs = hid_enumerate(0x1532, 0x00A6);
    struct hid_device_info* cur_dev = devs;

    int count = 0;
    while (cur_dev) {
        std::cout << "\nInterface " << cur_dev->interface_number
                  << " | Usage Page: 0x" << std::hex << cur_dev->usage_page
                  << " | Usage: 0x" << cur_dev->usage << std::dec << std::endl;

        analyzeInterface(cur_dev->path, cur_dev->interface_number);

        count++;
        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);

    std::cout << "\n========================================" << std::endl;
    std::cout << "Analyzed " << count << " interfaces total" << std::endl;
    std::cout << "========================================" << std::endl;

    hid_exit();
    return 0;
}
