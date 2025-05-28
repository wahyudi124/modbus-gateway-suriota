#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Constants.h"
#include "Cache.h"

// File operation functions
void createRecord(const char* filePath, JsonVariant data);
void readById(const char* filePath, int id);
void readPaginated(const char* filePath, int page, int pageSize);
void readAllDeviceNames(const char* filePath);
void readAllModbusConfigNames(const char* filePath);
void updateRecord(const char* filePath, int id, JsonVariant data);
void updateConfig(const char* filePath, JsonVariant data);
void readConfig(const char* filePath);
void updateLoggingConfig(const char* filePath, JsonVariant data);
void readLoggingConfig(const char* filePath);
void updateList(const char* filePath, JsonVariant data);
void readList(const char* filePath);
void deleteRecord(const char* filePath, int id);

// Implementation of file operation functions
inline void createRecord(const char* filePath, JsonVariant data) {
  DynamicJsonDocument doc(8192);
  
  doc.to<JsonArray>();
  
  if (!loadFromCache(filePath, doc)) {
    File file = LittleFS.open(filePath, "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (error) {
        Serial.println("Error parsing existing file, using empty array");
        doc.clear();
        doc.to<JsonArray>();
      }
      file.close();
    }
  }

  JsonArray array = doc.as<JsonArray>();
  
  int maxId = 0;
  for (JsonVariant v : array) {
    if (v.containsKey("id") && v["id"].is<int>() && v["id"].as<int>() > maxId) {
      maxId = v["id"].as<int>();
    }
  }
  JsonObject newData = data.as<JsonObject>();
  newData["id"] = maxId + 1;
  array.add(newData);

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    sendResponse("Error writing to file");
    return;
  }
  
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
    sendResponse("Error writing to file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("Record created successfully");
}

inline void readById(const char* filePath, int id) {
  DynamicJsonDocument doc(8192);
  if (loadFromCache(filePath, doc)) {
    DynamicJsonDocument response(1024);
    JsonArray responseArray = response.to<JsonArray>();

    for (JsonVariant v : doc.as<JsonArray>()) {
      if (v["id"].is<int>() && v["id"].as<int>() == id) {
        responseArray.add(v);
        break;
      }
    }

    if (responseArray.size() == 0) {
      sendResponse("Record not found");
      return;
    }

    String output;
    serializeJson(responseArray, output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("File not found");
    sendResponse("File not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing file");
    sendResponse("Error parsing file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);

  DynamicJsonDocument response(1024);
  JsonArray responseArray = response.to<JsonArray>();

  for (JsonVariant v : doc.as<JsonArray>()) {
    if (v["id"].is<int>() && v["id"].as<int>() == id) {
      responseArray.add(v);
      break;
    }
  }

  if (responseArray.size() == 0) {
    sendResponse("Record not found");
    return;
  }

  serializeJson(responseArray, output);
  sendResponse(output);
}

inline void readPaginated(const char* filePath, int page, int pageSize) {
  DynamicJsonDocument doc(16384);
  if (loadFromCache(filePath, doc)) {
    JsonArray allRecords = doc.as<JsonArray>();
    int totalRecords = allRecords.size();
    Serial.print("Total records (cache): ");
    Serial.println(totalRecords);

    if (totalRecords == 0) {
      sendResponse("No records available");
      return;
    }

    int startIndex = (page - 1) * pageSize;
    int endIndex = min(startIndex + pageSize, totalRecords);

    if (startIndex >= totalRecords) {
      Serial.println("Page out of range");
      sendResponse("Page out of range");
      return;
    }

    DynamicJsonDocument response(16384);
    JsonObject result = response.createNestedObject();
    JsonArray data = result.createNestedArray("data");

    for (int i = startIndex; i < endIndex; i++) {
      data.add(allRecords[i]);
    }

    result["page"] = page;
    result["pageSize"] = pageSize;
    result["totalRecords"] = totalRecords;
    result["totalPages"] = (totalRecords + pageSize - 1) / pageSize;

    String output;
    serializeJson(result, output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("File not found");
    sendResponse("File not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing file");
    sendResponse("Error parsing file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);

  JsonArray allRecords = doc.as<JsonArray>();
  int totalRecords = allRecords.size();
  Serial.print("Total records: ");
  Serial.println(totalRecords);

  if (totalRecords == 0) {
    sendResponse("No records available");
    return;
  }

  int startIndex = (page - 1) * pageSize;
  int endIndex = min(startIndex + pageSize, totalRecords);

  if (startIndex >= totalRecords) {
    Serial.println("Page out of range");
    sendResponse("Page out of range");
    return;
  }

  DynamicJsonDocument response(16384);
  JsonObject result = response.createNestedObject();
  JsonArray data = result.createNestedArray("data");

  for (int i = startIndex; i < endIndex; i++) {
    data.add(allRecords[i]);
  }

  result["page"] = page;
  result["pageSize"] = pageSize;
  result["totalRecords"] = totalRecords;
  result["totalPages"] = (totalRecords + pageSize - 1) / pageSize;

  serializeJson(result, output);
  sendResponse(output);
}

inline void readAllDeviceNames(const char* filePath) {
  DynamicJsonDocument doc(8192);
  if (loadFromCache(filePath, doc)) {
    DynamicJsonDocument response(8192);
    JsonArray names = response.to<JsonArray>();

    for (JsonVariant v : doc.as<JsonArray>()) {
      if (v.containsKey("name") && v["name"].is<String>()) {
        names.add(v["name"].as<String>());
      }
    }

    String output;
    serializeJson(names, output);
    Serial.print("Device names (cache): ");
    Serial.println(output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Devices file not found");
    sendResponse("Devices file not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing devices file");
    sendResponse("Error parsing devices file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);

  DynamicJsonDocument response(8192);
  JsonArray names = response.to<JsonArray>();

  for (JsonVariant v : doc.as<JsonArray>()) {
    if (v.containsKey("name") && v["name"].is<String>()) {
      names.add(v["name"].as<String>());
    }
  }

  serializeJson(names, output);
  Serial.print("Device names: ");
  Serial.println(output);
  sendResponse(output);
}

inline void readAllModbusConfigNames(const char* filePath) {
  DynamicJsonDocument doc(8192);
  if (loadFromCache(filePath, doc)) {
    DynamicJsonDocument response(8192);
    JsonArray names = response.to<JsonArray>();

    for (JsonVariant v : doc.as<JsonArray>()) {
      if (v.containsKey("name") && v["name"].is<String>()) {
        names.add(v["name"].as<String>());
      }
    }

    String output;
    serializeJson(names, output);
    Serial.print("Modbus config names (cache): ");
    Serial.println(output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Modbus config file not found");
    sendResponse("Modbus config file not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing modbus config file");
    sendResponse("Error parsing modbus config file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);

  DynamicJsonDocument response(8192);
  JsonArray names = response.to<JsonArray>();

  for (JsonVariant v : doc.as<JsonArray>()) {
    if (v.containsKey("name") && v["name"].is<String>()) {
      names.add(v["name"].as<String>());
    }
  }

  serializeJson(names, output);
  Serial.print("Modbus config names: ");
  Serial.println(output);
  sendResponse(output);
}

inline void updateRecord(const char* filePath, int id, JsonVariant data) {
  DynamicJsonDocument doc(8192);
  if (!loadFromCache(filePath, doc)) {
    File file = LittleFS.open(filePath, "r");
    if (!file) {
      Serial.println("File not found");
      sendResponse("File not found");
      return;
    }
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.println("Error parsing file");
      sendResponse("Error parsing file");
      file.close();
      return;
    }
    file.close();
  }

  bool found = false;
  for (JsonVariant v : doc.as<JsonArray>()) {
    if (v["id"].is<int>() && v["id"].as<int>() == id) {
      JsonObject updatedData = data.as<JsonObject>();
      updatedData["id"] = id;
      v.set(updatedData);
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.print("Record with ID ");
    Serial.print(id);
    Serial.println(" not found");
    sendResponse("Record not found");
    return;
  }

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    sendResponse("Error writing to file");
    return;
  }
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
    sendResponse("Error writing to file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("Record updated successfully");
}

inline void updateConfig(const char* filePath, JsonVariant data) {
  DynamicJsonDocument doc(8192);
  JsonObject config = doc.to<JsonObject>();
  
  JsonObject updatedData = data.as<JsonObject>();
  config.set(updatedData);

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    sendResponse("Error writing to config file");
    return;
  }
  
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to config file");
    sendResponse("Error writing to config file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("Config updated successfully");
}

inline void readConfig(const char* filePath) {
  DynamicJsonDocument doc(8192);
  if (loadFromCache(filePath, doc)) {
    String output;
    serializeJson(doc, output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Config file not found");
    sendResponse("Config file not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing config file");
    sendResponse("Error parsing config file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);
  sendResponse(output);
}

inline void updateLoggingConfig(const char* filePath, JsonVariant data) {
  DynamicJsonDocument doc(1024);
  JsonObject config = doc.to<JsonObject>();
  
  JsonObject updatedData = data.as<JsonObject>();
  config.set(updatedData);

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open logging config file for writing");
    sendResponse("Error writing to logging config file");
    return;
  }
  
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to logging config file");
    sendResponse("Error writing to logging config file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("Logging config updated successfully");
}

inline void readLoggingConfig(const char* filePath) {
  DynamicJsonDocument doc(1024);
  if (loadFromCache(filePath, doc)) {
    String output;
    serializeJson(doc, output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Logging config file not found");
    sendResponse("Logging config file not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing logging config file");
    sendResponse("Error parsing logging config file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);
  sendResponse(output);
}

inline void updateList(const char* filePath, JsonVariant data) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  
  JsonArray items = data["items"].as<JsonArray>();
  for (JsonVariant item : items) {
    array.add(item.as<String>());
  }

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    sendResponse("Error writing to file");
    return;
  }
  
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
    sendResponse("Error writing to file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("List updated successfully");
}

inline void readList(const char* filePath) {
  DynamicJsonDocument doc(1024);
  if (loadFromCache(filePath, doc)) {
    String output;
    serializeJson(doc, output);
    Serial.print("List content (cache): ");
    Serial.println(output);
    sendResponse(output);
    return;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("List file not found");
    sendResponse("List file not found");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Error parsing list file");
    sendResponse("Error parsing list file");
    file.close();
    return;
  }
  file.close();

  String output;
  serializeJson(doc, output);
  updateCache(filePath, output);
  Serial.print("List content: ");
  Serial.println(output);
  sendResponse(output);
}

inline void deleteRecord(const char* filePath, int id) {
  DynamicJsonDocument doc(8192);
  if (!loadFromCache(filePath, doc)) {
    File file = LittleFS.open(filePath, "r");
    if (!file) {
      Serial.println("File not found");
      sendResponse("File not found");
      return;
    }
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.println("Error parsing file");
      sendResponse("Error parsing file");
      file.close();
      return;
    }
    file.close();
  }

  JsonArray array = doc.as<JsonArray>();
  for (size_t i = 0; i < array.size(); i++) {
    if (array[i]["id"].is<int>() && array[i]["id"].as<int>() == id) {
      array.remove(i);
      break;
    }
  }

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    sendResponse("Error writing to file");
    return;
  }
  String output;
  if (serializeJson(doc, output) == 0 || serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
    sendResponse("Error writing to file");
    file.close();
    return;
  }
  file.close();

  updateCache(filePath, output);
  sendResponse("Record deleted successfully");
}

#endif // FILE_OPERATIONS_H