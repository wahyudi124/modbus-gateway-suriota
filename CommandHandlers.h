#ifndef COMMAND_HANDLERS_H
#define COMMAND_HANDLERS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Constants.h"
#include "FileOperations.h"

// Forward declaration
void sendResponse(String response);

// Command handler functions
void processCommand(String command);
void handleModbusData(String action, JsonDocument& doc);
void handleDevicesData(String action, JsonDocument& doc);
void handleConfigData(String action, JsonDocument& doc);
void handleLoggingConfigData(String action, JsonDocument& doc);
void handleClistData(String action, JsonDocument& doc);
void handleLglistData(String action, JsonDocument& doc);

// Implementation of command handler functions
inline void processCommand(String command) {
  // Note: Mutex handling is now done in fileTask
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, command);

  if (error) {
    sendResponse("Error parsing JSON");
    return;
  }

  String action = doc["action"];
  String dataset = doc["dataset"];

  Serial.print("Processing command - Action: ");
  Serial.print(action);
  Serial.print(", Dataset: ");
  Serial.println(dataset);

  if (dataset == "modbus") {
    handleModbusData(action, doc);
  } else if (dataset == "devices") {
    handleDevicesData(action, doc);
  } else if (dataset == "config") {
    handleConfigData(action, doc);
  } else if (dataset == "logging_config") {
    handleLoggingConfigData(action, doc);
  } else if (dataset == "clist") {
    handleClistData(action, doc);
  } else if (dataset == "lglist") {
    handleLglistData(action, doc);
  } else {
    sendResponse("Invalid dataset");
  }
}

inline void handleModbusData(String action, JsonDocument& doc) {
  if (action == "CREATE") {
    createRecord(MODBUS_CONFIG_PATH, doc["data"]);
  } else if (action == "READ") {
    if (doc.containsKey("names") && doc["names"].as<bool>()) {
      readAllModbusConfigNames(MODBUS_CONFIG_PATH);
    } else if (doc.containsKey("id")) {
      if (!doc["id"].is<int>()) {
        sendResponse("Error: ID must be an integer");
        return;
      }
      readById(MODBUS_CONFIG_PATH, doc["id"].as<int>());
    } else {
      int page = doc["page"] | 1;
      int pageSize = doc["pageSize"] | 10;
      readPaginated(MODBUS_CONFIG_PATH, page, pageSize);
    }
  } else if (action == "UPDATE") {
    if (!doc["data"].containsKey("id")) {
      sendResponse("Error: ID is required in data object");
      return;
    }
    if (!doc["data"]["id"].is<int>()) {
      sendResponse("Error: ID must be an integer");
      return;
    }
    int id = doc["data"]["id"].as<int>();
    Serial.print("Extracted ID for UPDATE: ");
    Serial.println(id);
    updateRecord(MODBUS_CONFIG_PATH, id, doc["data"]);
  } else if (action == "DELETE") {
    if (!doc["data"].containsKey("id")) {
      sendResponse("Error: ID is required in data object");
      return;
    }
    if (!doc["data"]["id"].is<int>()) {
      sendResponse("Error: ID must be an integer");
      return;
    }
    int id = doc["data"]["id"].as<int>();
    Serial.print("Extracted ID for DELETE: ");
    Serial.println(id);
    deleteRecord(MODBUS_CONFIG_PATH, id);
  } else {
    sendResponse("Invalid action");
  }
}

inline void handleDevicesData(String action, JsonDocument& doc) {
  if (action == "CREATE") {
    createRecord(DEVICES_PATH, doc["data"]);
  } else if (action == "READ") {
    if (doc.containsKey("names") && doc["names"].as<bool>()) {
      readAllDeviceNames(DEVICES_PATH);
    } else if (doc.containsKey("id")) {
      if (!doc["id"].is<int>()) {
        sendResponse("Error: ID must be an integer");
        return;
      }
      readById(DEVICES_PATH, doc["id"].as<int>());
    } else {
      int page = doc["page"] | 1;
      int pageSize = doc["pageSize"] | 10;
      readPaginated(DEVICES_PATH, page, pageSize);
    }
  } else if (action == "UPDATE") {
    if (!doc["data"].containsKey("id")) {
      sendResponse("Error: ID is required in data object");
      return;
    }
    if (!doc["data"]["id"].is<int>()) {
      sendResponse("Error: ID must be an integer");
      return;
    }
    int id = doc["data"]["id"].as<int>();
    Serial.print("Extracted ID for UPDATE: ");
    Serial.println(id);
    updateRecord(DEVICES_PATH, id, doc["data"]);
  } else if (action == "DELETE") {
    if (!doc["data"].containsKey("id")) {
      sendResponse("Error: ID is required in data object");
      return;
    }
    if (!doc["data"]["id"].is<int>()) {
      sendResponse("Error: ID must be an integer");
      return;
    }
    int id = doc["data"]["id"].as<int>();
    Serial.print("Extracted ID for DELETE: ");
    Serial.println(id);
    deleteRecord(DEVICES_PATH, id);
  } else {
    sendResponse("Invalid action");
  }
}

inline void handleConfigData(String action, JsonDocument& doc) {
  if (action == "UPDATE") {
    updateConfig(CONFIG_PATH, doc["data"]);
  } else if (action == "READ") {
    readConfig(CONFIG_PATH);
  } else {
    sendResponse("Invalid action for config dataset: Only UPDATE and READ are supported");
  }
}

inline void handleLoggingConfigData(String action, JsonDocument& doc) {
  if (action == "UPDATE") {
    updateLoggingConfig(LOGGING_CONFIG_PATH, doc["data"]);
  } else if (action == "READ") {
    readLoggingConfig(LOGGING_CONFIG_PATH);
  } else {
    sendResponse("Invalid action for logging_config dataset: Only UPDATE and READ are supported");
  }
}

inline void handleClistData(String action, JsonDocument& doc) {
  if (action == "UPDATE") {
    updateList(CLIST_PATH, doc["data"]);
  } else if (action == "READ") {
    readList(CLIST_PATH);
  } else {
    sendResponse("Invalid action for clist dataset: Only UPDATE and READ are supported");
  }
}

inline void handleLglistData(String action, JsonDocument& doc) {
  if (action == "UPDATE") {
    updateList(LGLIST_PATH, doc["data"]);
  } else if (action == "READ") {
    readList(LGLIST_PATH);
  } else {
    sendResponse("Invalid action for lglist dataset: Only UPDATE and READ are supported");
  }
}

#endif // COMMAND_HANDLERS_H