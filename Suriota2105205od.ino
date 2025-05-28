#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "Constants.h"
#include "Cache.h"
#include "BLEHandlers.h"
#include "FileOperations.h"
#include "CommandHandlers.h"
#include "FreeRTOSTasks.h"

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

// FreeRTOS handles
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t fileTaskHandle = NULL;
SemaphoreHandle_t fileMutex = NULL;
QueueHandle_t commandQueue = NULL;

void setup() {
  Serial.begin(115200);
  
  // Create FreeRTOS resources
  fileMutex = xSemaphoreCreateMutex();
  commandQueue = xQueueCreate(10, sizeof(CommandData));
  
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  
  // Initialize files
  initFiles();
  
  // Create tasks
  xTaskCreatePinnedToCore(
    bleTask,          // Task function
    "BLE Task",       // Name
    8192,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &bleTaskHandle,   // Task handle
    1                 // Core (1 = Arduino loop core)
  );
  
  xTaskCreatePinnedToCore(
    fileTask,         // Task function
    "File Task",      // Name
    8192,             // Stack size
    NULL,             // Parameters
    2,                // Priority
    &fileTaskHandle,  // Task handle
    0                 // Core (0 = free core)
  );
}

void loop() {
  // Empty loop - tasks handle everything
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}