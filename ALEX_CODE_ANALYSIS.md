# Analyse av Alex Perathoner's Razer Battery Code

## Kilde
https://github.com/AlexPerathoner/razer-battery-menu-bar-macos

## Nøkkelfunn

### 1. De bruker librazermacos (høyere-nivå API)
- **Ikke direkte HIDAPI**: De bruker `razer_attr_read_get_battery()` funksjonen
- **Abstraksjon**: Dette er en wrapper rundt HID-kommunikasjonen
- **librazermacos mappen er tom**: Biblioteket er ikke inkludert i repositoriet

### 2. Batteri-henting implementasjon
```c
char battery_level[10];
ssize_t result = razer_attr_read_get_battery(device.usbDevice, battery_level);
if (result > 0) {
    int battery_level_raw = (unsigned char)battery_level[0]; // Første byte!
    int battery_level_percent = (battery_level_raw * 100) / 255;
    return battery_level_percent;
}
```

**Viktige observasjoner:**
- De leser fra `battery_level[0]` (første byte i buffer)
- Skalering: 0-255 → 0-100 (samme som vi gjør)
- De bruker `device.usbDevice` (en IOKit device, ikke HIDAPI direkte)

### 3. Wireless device detection
```c
int is_device_wireless(UInt16 productId) {
    return (productId == USB_DEVICE_ID_RAZER_VIPER_ULTIMATE_WIRELESS) ||
        (productId == USB_DEVICE_ID_RAZER_DEATHADDER_V2_PRO_WIRELESS);
    // ... flere modeller
}
```

**Viper V2 Pro er IKKE i listen!** De støtter:
- Viper Ultimate Wireless
- DeathAdder V2 Pro Wireless
- Men **IKKE** Viper V2 Pro (0x00A6)

### 4. Device enumeration
```c
RazerDevices allDevices = getAllRazerDevices();
for (int i = 0; i < allDevices.size; i++) {
    RazerDevice device = razerDevices[i];
    if (is_device_wireless(device.productId)) {
        // Hent batteri
    }
}
```

De enumererer alle Razer-enheter og filtrerer på wireless.

## Hva vi kan lære

### 1. librazermacos bruker IOKit, ikke HIDAPI direkte
- `device.usbDevice` er sannsynligvis en `IOHIDDeviceRef` eller lignende
- `razer_attr_read_get_battery()` håndterer all protokoll-kommunikasjonen

### 2. Batteri-data ligger i første byte
- De leser `battery_level[0]` direkte
- Dette tyder på at librazermacos allerede har parset responsen

### 3. Viper V2 Pro støttes ikke direkte
- Alex sin kode støtter ikke Viper V2 Pro (0x00A6)
- Vi må finne protokollen selv eller bruke librazermacos hvis det støtter det

## Neste steg

### Alternativ 1: Finne librazermacos kildekode
- Søk etter "librazermacos" eller "razer-macos" på GitHub
- Se hvordan `razer_attr_read_get_battery()` faktisk fungerer
- Implementer samme logikk i vår kode

### Alternativ 2: Bruke IOKit direkte
- I stedet for HIDAPI, bruk IOKit (som librazermacos sannsynligvis gjør)
- Dette kan gi bedre tilgang til device attributes

### Alternativ 3: Fortsette med vår tilnærming
- Vi er på rett spor med HIDAPI
- Problemet er at vi ikke får data tilbake
- Kanskje vi trenger å bruke IOKit for å lese device attributes i stedet for feature reports?

## Konklusjon

Alex sin kode viser at:
1. ✅ Det ER mulig å få batteridata fra Razer-mus på macOS
2. ⚠️ De bruker en høyere-nivå API (librazermacos) som vi ikke har tilgang til
3. ⚠️ Viper V2 Pro støttes ikke direkte i deres kode
4. 💡 Vi må enten finne librazermacos eller implementere samme logikk selv

**Vår tilnærming er riktig**, men vi mangler kanskje riktig protokoll-struktur eller må bruke IOKit i stedet for HIDAPI.

