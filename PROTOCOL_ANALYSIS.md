# Protokoll-analyse fra librazermacos

## Kilde
https://github.com/1kc/librazermacos

## Nøkkelfunn

### 1. Battery Query Funksjon
**Fil**: `src/lib/razermouse_driver.c`, linje 1780-1811

```c
ssize_t razer_attr_read_get_battery(IOUSBDeviceInterface **usb_dev, char *buf)
{
    struct razer_report report = razer_chroma_misc_get_battery_level();
    struct razer_report response_report = {0};
    
    // Transaction ID varierer basert på enhet
    switch (product) {
        case USB_DEVICE_ID_RAZER_LANCEHEAD_WIRED:
        case USB_DEVICE_ID_RAZER_DEATHADDER_V2_PRO_WIRED:
            report.transaction_id.id = 0x3f;  // Eldre enheter
            break;
        case USB_DEVICE_ID_RAZER_BASILISK_ULTIMATE_RECEIVER:
        case USB_DEVICE_ID_RAZER_OROCHI_V2_RECEIVER:
            report.transaction_id.id = 0x1f;  // Nyere wireless enheter
            break;
    }
    
    response_report = razer_send_payload(usb_dev, &report);
    
    // BATTERI DATA LIGGER I arguments[1] !!!
    return sprintf(buf, "%d\n", response_report.arguments[1]);
}
```

### 2. Battery Level Command Konstruksjon
**Fil**: `src/lib/razerchromacommon.c`, linje 978-981

```c
struct razer_report razer_chroma_misc_get_battery_level(void)
{
    return get_razer_report(0x07, 0x80, 0x02);
    //                       │     │     │
    //                       │     │     └── Data Size: 0x02
    //                       │     └── Command ID: 0x80
    //                       └── Command Class: 0x07
}
```

### 3. Report Struktur
**Fil**: `src/include/razercommon.h`, linje 97-108

```c
struct razer_report {
    unsigned char status;                      // Byte 0
    union transaction_id_union transaction_id; // Byte 1
    unsigned short remaining_packets;          // Bytes 2-3 (Big Endian)
    unsigned char protocol_type;               // Byte 4 (alltid 0x00)
    unsigned char data_size;                   // Byte 5
    unsigned char command_class;               // Byte 6
    union command_id_union command_id;         // Byte 7
    unsigned char arguments[80];               // Bytes 8-87
    unsigned char crc;                         // Byte 88
    unsigned char reserved;                    // Byte 89 (alltid 0x00)
};
```

### 4. Default Report Initialisering
**Fil**: `src/lib/razercommon.c`, linje 91-103

```c
struct razer_report get_razer_report(unsigned char command_class, unsigned char command_id, unsigned char data_size) {
    new_report.status = 0x00;
    new_report.transaction_id.id = 0xFF;  // Default, kan overstyres
    new_report.remaining_packets = 0x00;
    new_report.protocol_type = 0x00;
    new_report.command_class = command_class;
    new_report.command_id.id = command_id;
    new_report.data_size = data_size;
}
```

### 5. USB Kommunikasjon (VIKTIG!)
**Fil**: `src/lib/razercommon.c`, linje 17-28 og 48-85

De bruker **USB Control Messages**, IKKE HID Feature Reports:

```c
// SEND REQUEST
request.bRequest = HID_REQ_SET_REPORT;  // 0x09
request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT;  // 0x21
request.wValue = 0x300;
request.wIndex = report_index;  // Vanligvis 0x02
request.wLength = RAZER_USB_REPORT_LEN;  // 90 bytes

// GET RESPONSE
request.bRequest = HID_REQ_GET_REPORT;  // 0x01
request.bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN;  // 0xA1
request.wValue = 0x300;
request.wIndex = report_index;
request.wLength = RAZER_USB_REPORT_LEN;
```

### 6. Checksum Beregning
**Fil**: `src/lib/razercommon.c`, linje 124-138

```c
// XOR bytes 2-87 (indices 2 to 87, totalt 86 bytes)
for(i = 2; i < 88; i++) {
    crc ^= _report[i];
}
```

**VIKTIG**: De XOR-er bytes 2-87, ikke 2-88 som vi gjør!

---

## EKSTRAHERTE PROTOKOLL-VERDIER

### For Battery Query

| Parameter | Verdi | Beskrivelse |
|-----------|-------|-------------|
| Command Class | `0x07` | Power/Misc |
| Command ID | `0x80` | Get Battery Level |
| Data Size | `0x02` | 2 bytes respons |
| Transaction ID | `0x1F` | For Viper-lignende enheter |
| Status | `0x00` | New Command |

### Report Byte Layout (90 bytes)

| Byte | Innhold | Vår Verdi |
|------|---------|-----------|
| 0 | Status | 0x00 |
| 1 | Transaction ID | 0x1F |
| 2-3 | Remaining Packets | 0x0000 |
| 4 | Protocol Type | 0x00 |
| 5 | Data Size | 0x02 |
| 6 | Command Class | 0x07 |
| 7 | Command ID | 0x80 |
| 8-87 | Arguments | 0x00... |
| 88 | CRC | XOR(bytes 2-87) |
| 89 | Reserved | 0x00 |

### Respons Battery Data Lokasjon

**Battery er i `arguments[1]` = Byte 9 av responsen!**

Men de bruker `response_report.arguments[1]`, som er byte offset 9 fra starten (etter de 8 header-bytes).

---

## KRITISK FORSKJELL FRA VÅR IMPLEMENTERING

### 1. USB Control Messages vs HID Feature Reports

De bruker **IOKit USB Control Messages**:
- `DeviceRequest()` med `HID_REQ_SET_REPORT` (0x09) og `HID_REQ_GET_REPORT` (0x01)
- `wValue = 0x300` (Report Type 3 = Feature Report, Report ID 0)
- `wIndex = 0x02` (Interface 2)

Vi bruker **HIDAPI Feature Reports**:
- `hid_send_feature_report()` og `hid_get_feature_report()`
- Report ID er prepended til bufferen

### 2. Checksum Range

De: XOR bytes 2-87 (`i < 88`)
Vi: XOR bytes 2-88 (`i <= 88`)

### 3. Byte Offsets

Deres `arguments[1]` = vår `response[9]` (etter Report ID stripping)

---

## ANBEFALINGER

1. **Prøv IOKit direkte** i stedet for HIDAPI
2. **Fiks checksum**: Endre til `i < 88` i stedet for `i <= 88`
3. **Battery offset** er korrekt: Byte 9 (arguments[1])
4. **Transaction ID** 0x1F er korrekt for wireless enheter

## Viper V2 Pro Support

**VIKTIG**: Viper V2 Pro (PID 0x00A6) er IKKE listet i librazermacos!

De støtter:
- Viper (0x0078)
- Viper 8KHz (0x0091)
- Viper Ultimate Wired (0x007A)
- Viper Ultimate Wireless (0x007B)
- Viper Mini (0x008A)

Men IKKE Viper V2 Pro (0x00A6), som er en nyere enhet.

Dette kan bety at Viper V2 Pro bruker en **annen protokoll** eller **andre Transaction IDs**.

