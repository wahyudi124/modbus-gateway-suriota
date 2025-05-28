#ifndef CACHE_H
#define CACHE_H

#include <Arduino.h>
#include "Constants.h"

// Cache structure
struct CacheEntry {
  String path;
  String data;
  unsigned long timestamp;
  bool isValid;
};

// Global cache array for all configuration files
extern CacheEntry cache[];
extern const int CACHE_SIZE;

// Cache helper functions
CacheEntry* getCacheEntry(const char* filePath);
bool isCacheValid(CacheEntry* entry);
void invalidateCache(const char* filePath);
bool loadFromCache(const char* filePath, DynamicJsonDocument& doc);
void updateCache(const char* filePath, const String& data);
bool isFileEmpty(const char* filePath);

// Implementation of cache functions
inline CacheEntry* getCacheEntry(const char* filePath) {
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].path == filePath) {
      return &cache[i];
    }
  }
  return nullptr;
}

inline bool isCacheValid(CacheEntry* entry) {
  if (!entry || !entry->isValid) return false;
  unsigned long currentTime = millis();
  // Handle millis() overflow
  if (currentTime < entry->timestamp) {
    entry->isValid = false;
    return false;
  }
  return (currentTime - entry->timestamp) < CACHE_EXPIRY_MS;
}

inline void invalidateCache(const char* filePath) {
  CacheEntry* entry = getCacheEntry(filePath);
  if (entry) {
    entry->isValid = false;
    entry->data = "";
    entry->timestamp = 0;
    Serial.print("Invalidated cache for ");
    Serial.println(filePath);
  }
}

inline bool loadFromCache(const char* filePath, DynamicJsonDocument& doc) {
  CacheEntry* entry = getCacheEntry(filePath);
  if (entry && isCacheValid(entry)) {
    DeserializationError error = deserializeJson(doc, entry->data);
    if (!error) {
      Serial.print("Loaded from cache: ");
      Serial.println(filePath);
      return true;
    }
    Serial.print("Error parsing cached data for ");
    Serial.println(filePath);
    invalidateCache(filePath);
  }
  return false;
}

inline void updateCache(const char* filePath, const String& data) {
  CacheEntry* entry = getCacheEntry(filePath);
  if (entry) {
    entry->data = data;
    entry->timestamp = millis();
    entry->isValid = true;
    Serial.print("Updated cache for ");
    Serial.println(filePath);
  }
}

inline bool isFileEmpty(const char* filePath) {
  CacheEntry* entry = getCacheEntry(filePath);
  if (entry && isCacheValid(entry)) {
    return entry->data.length() == 0 || entry->data == "{}" || entry->data == "[]";
  }
  
  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return true;
  }
  String content = file.readString();
  file.close();
  content.trim();
  return content.length() == 0 || content == "{}" || content == "[]";
}

#endif // CACHE_H