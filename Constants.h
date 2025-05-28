#ifndef CONSTANTS_H
#define CONSTANTS_H

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// File paths
#define MODBUS_CONFIG_PATH "/modbus_config.json"
#define DEVICES_PATH "/devices.json"
#define CONFIG_PATH "/com_config.json"
#define LOGGING_CONFIG_PATH "/logging_config.json"
#define CLIST_PATH "/clist.json"
#define LGLIST_PATH "/lglist.json"

// Cache settings
#define CACHE_EXPIRY_MS 86400000 // 1 day in milliseconds

#endif // CONSTANTS_H