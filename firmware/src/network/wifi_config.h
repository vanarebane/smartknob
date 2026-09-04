#pragma once

// Hardcoded WiFi credentials used only in OTA mode. Fill in before flashing.
#define WIFI_SSID "Meie wifi 5Ghz"
#define WIFI_PASS "maeiteapeast"

// mDNS hostname: OTA upload and console reachable at smartknob.local
#define OTA_HOSTNAME "smartknob"
// Must match --auth in platformio.ini [env:view-ota] upload_flags
#define OTA_PASSWORD "smartknob"

// TCP port serving the interactive plaintext protocol (logs, 'C' calibrate, space = demo config)
#define OTA_CONSOLE_PORT 23

// UDP broadcast port for the "SMARTKNOB-OTA <ip>" discovery beacon
#define OTA_BEACON_PORT 43232

// Failsafes: both reboot back into normal BLE mode
#define OTA_WIFI_CONNECT_TIMEOUT_MS 20000
#define OTA_IDLE_TIMEOUT_MS 600000
