#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
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

// Forward declaration
class ModbusHandler;
extern ModbusHandler modbusHandler;

// FreeRTOS handles
extern TaskHandle_t bleTaskHandle;
extern TaskHandle_t fileTaskHandle;
extern TaskHandle_t modbusTaskHandle;
extern SemaphoreHandle_t fileMutex;
extern QueueHandle_t commandQueue;

// Command structure for queue
struct CommandData {
  String command;
  CommandData() : command("") {}
  CommandData(String cmd) : command(cmd) {}
};

// Task function prototypes
void bleTask(void *parameter);
void fileTask(void *parameter);
void modbusTask(void *parameter);
void initFiles();
void loadAllConfigToCache();

// Queue command function
void queueCommand(String command) {
  CommandData cmdData(command);
  xQueueSend(commandQueue, &cmdData, portMAX_DELAY);
}

// BLE Task implementation
void bleTask(void *parameter) {
  // Initialize BLE
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
  
  // BLE task loop
  for(;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// File Task implementation
void fileTask(void *parameter) {
  CommandData cmdData;
  
  for(;;) {
    // Wait for commands from the queue
    if (xQueueReceive(commandQueue, &cmdData, portMAX_DELAY) == pdTRUE) {
      // Take mutex before accessing files
      if (xSemaphoreTake(fileMutex, portMAX_DELAY) == pdTRUE) {
        // Process the command
        processCommand(cmdData.command);
        // Release mutex
        xSemaphoreGive(fileMutex);
      }
    }
  }
}

// Initialize files with mutex protection
void initFiles() {
  if (xSemaphoreTake(fileMutex, portMAX_DELAY) == pdTRUE) {
    // Initialize configuration files if they don't exist
    const char* paths[] = {DEVICES_PATH, MODBUS_CONFIG_PATH, CLIST_PATH, LGLIST_PATH};
    for (const char* path : paths) {
      if (!LittleFS.exists(path)) {
        File file = LittleFS.open(path, "w");
        if (!file) {
          Serial.print("Failed to create ");
          Serial.println(path);
          xSemaphoreGive(fileMutex);
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
        xSemaphoreGive(fileMutex);
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
        xSemaphoreGive(fileMutex);
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
    
    xSemaphoreGive(fileMutex);
  }
}

// Load all configuration files to cache
void loadAllConfigToCache() {
  if (xSemaphoreTake(fileMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("Loading all configurations to cache...");
    
    // Array of all configuration file paths
    const char* configPaths[] = {
      DEVICES_PATH,
      MODBUS_CONFIG_PATH,
      CONFIG_PATH,
      LOGGING_CONFIG_PATH,
      CLIST_PATH,
      LGLIST_PATH
    };
    
    // Load each file into cache
    for (const char* path : configPaths) {
      if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        if (file) {
          String content = file.readString();
          file.close();
          updateCache(path, content);
          Serial.print("Loaded to cache: ");
          Serial.println(path);
        } else {
          Serial.print("Failed to open file for reading: ");
          Serial.println(path);
        }
      } else {
        Serial.print("File does not exist: ");
        Serial.println(path);
      }
    }
    
    xSemaphoreGive(fileMutex);
    Serial.println("All configurations loaded to cache");
  }
}

// Modbus Task implementation
void modbusTask(void *parameter);

#endif // FREERTOS_TASKS_H