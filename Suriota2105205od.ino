#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "Constants.h"
#include "Cache.h"
#include "BLEHandlers.h"
#include "FileOperations.h"
#include "CommandHandlers.h"

// Implement Chahe Config Load

// Global cache array for all configuration files
CacheEntry cache[] = {
  {MODBUS_CONFIG_PATH, "", 0, false},
  {DEVICES_PATH, "", 0, false},
  {CONFIG_PATH, "", 0, false},
  {LOGGING_CONFIG_PATH, "", 0, false},
  {CLIST_PATH, "", 0, false},
  {LGLIST_PATH, "", 0, false}
};
const int CACHE_SIZE = 6;

// Global BLE variables
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

void setup() {
  Serial.begin(115200);
  
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  
  // Initialize configuration files if they don't exist
  const char* paths[] = {DEVICES_PATH, MODBUS_CONFIG_PATH, CLIST_PATH, LGLIST_PATH};
  for (const char* path : paths) {
    if (!LittleFS.exists(path)) {
      File file = LittleFS.open(path, "w");
      if (!file) {
        Serial.print("Failed to create ");
        Serial.println(path);
        return;
      }
      file.print("[]");
      file.close();
      Serial.print("Created empty ");
      Serial.println(path);
    }
  }

  if (!LittleFS.exists(CONFIG_PATH)) {
    File file = LittleFS.open(CONFIG_PATH, "w");
    if (!file) {
      Serial.println("Failed to create config file");
      return;
    }
    DynamicJsonDocument doc(8192);
    JsonObject config = doc.to<JsonObject>();
    JsonObject communication = config.createNestedObject("communication");
    communication["type"] = "ETH";
    communication["mode"] = "A";
    communication["ip"] = "192.168.0.1";
    communication["mac"] = "20202020";
    communication["ssid"] = "TEST";
    communication["pass"] = "12345678";
    JsonObject protocol = config.createNestedObject("protocol");
    protocol["type"] = "MQTT";
    protocol["server"] = "demo.thingsboard.org";
    protocol["port"] = 1883;
    protocol["topic"] = "/data";
    protocol["clientid"] = "test";
    protocol["qos"] = 1;
    JsonObject interval = config.createNestedObject("interval");
    interval["time"] = 10;
    interval["type"] = "s";
    JsonObject auth = config.createNestedObject("auth");
    auth["type"] = "BASIC";
    auth["username"] = "admin";
    auth["password"] = "12345";
    String output;
    serializeJson(doc, output);
    updateCache(CONFIG_PATH, output);
    serializeJson(doc, file);
    file.close();
    Serial.println("Created default config file");
  }

  if (!LittleFS.exists(LOGGING_CONFIG_PATH)) {
    File file = LittleFS.open(LOGGING_CONFIG_PATH, "w");
    if (!file) {
      Serial.println("Failed to create logging config file");
      return;
    }
    DynamicJsonDocument doc(1024);
    JsonObject config = doc.to<JsonObject>();
    config["retention"] = "1w";
    config["interval"] = "5m";
    String output;
    serializeJson(doc, output);
    updateCache(LOGGING_CONFIG_PATH, output);
    serializeJson(doc, file);
    file.close();
    Serial.println("Created default logging config file");
  }

  BLEDevice::init("ESP32_Modbus_Server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  uint16_t mtu = 512;
  BLEDevice::setMTU(mtu);
  Serial.print("MTU set to: ");
  Serial.println(BLEDevice::getMTU());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                     CHARACTERISTIC_UUID,
                     BLECharacteristic::PROPERTY_READ   |
                     BLECharacteristic::PROPERTY_WRITE  |
                     BLECharacteristic::PROPERTY_NOTIFY
                   );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();

  pServer->getAdvertising()->start();
  Serial.println("BLE Server started");
}

void loop() {
  delay(1000);
}