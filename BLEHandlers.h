#ifndef BLE_HANDLERS_H
#define BLE_HANDLERS_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "Constants.h"

// Forward declarations
void sendResponse(String response);
void queueCommand(String command);

// Global BLE variables
extern BLEServer* pServer;
extern BLECharacteristic* pCharacteristic;
extern bool deviceConnected;

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
        Serial.print("Negotiated MTU: ");
        Serial.println(BLEDevice::getMTU());
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();
    }
};

// BLE Characteristic Callbacks
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
private:
    String receivedData = "";
    unsigned long lastReceived = 0;
    const unsigned long TIMEOUT = 1000; // 1 second timeout

    void processBuffer() {
        if (receivedData.length() > 0 && (millis() - lastReceived > TIMEOUT)) {
            Serial.println("Timeout: Resetting buffer");
            receivedData = "";
            return;
        }

        int endPos;
        if ((endPos = receivedData.indexOf('#')) != -1) {
            String completeCommand = receivedData.substring(0, endPos);
            receivedData = "";
            
            Serial.println("Complete command received: ");
            Serial.println(completeCommand + "#");
            
            completeCommand.trim();
            
            DynamicJsonDocument doc(8192);
            String parts[30];
            int partCount = 0;
            
            String segments[15];
            int segmentCount = 0;
            int startPos = 0;
            int delimPos;
            
            while ((delimPos = completeCommand.indexOf('|', startPos)) != -1 && segmentCount < 15) {
                String segment = completeCommand.substring(startPos, delimPos);
                segment.trim();
                if (segment.length() > 0) {
                    segments[segmentCount++] = segment;
                }
                startPos = delimPos + 1;
            }
            if (startPos < completeCommand.length() && segmentCount < 15) {
                String segment = completeCommand.substring(startPos);
                segment.trim();
                if (segment.length() > 0) {
                    segments[segmentCount++] = segment;
                }
            }
            
            for (int i = 0; i < segmentCount; i++) {
                if (i < 2) {
                    Serial.print("Command part ");
                    Serial.print(i);
                    Serial.print(": '");
                    Serial.print(segments[i]);
                    Serial.println("'");
                    parts[partCount++] = segments[i];
                } else {
                    int colonPos = segments[i].indexOf(':');
                    if (colonPos != -1) {
                        String key = segments[i].substring(0, colonPos);
                        String value = segments[i].substring(colonPos + 1);
                        key.trim();
                        value.trim();
                        if (key.length() > 0) {
                            Serial.print("Found key: '");
                            Serial.print(key);
                            Serial.print("' with value: '");
                            Serial.print(value);
                            Serial.println("'");
                            parts[partCount++] = key;
                            if (value.length() > 0) {
                                parts[partCount++] = value;
                            }
                        }
                    } else {
                        segments[i].trim();
                        if (segments[i].length() > 0) {
                            Serial.print("Found parameter: '");
                            Serial.print(segments[i]);
                            Serial.println("'");
                            parts[partCount++] = segments[i];
                        }
                    }
                }
            }
            
            Serial.println("Parsed parts:");
            for (int i = 0; i < partCount; i++) {
                Serial.print(i);
                Serial.print(": ");
                Serial.println(parts[i]);
            }
            
            if (partCount >= 2) {
                doc["action"] = parts[0];
                doc["dataset"] = parts[1];
                
                if (parts[0] == "READ" && partCount > 2) {
                    for (int i = 2; i < partCount; i++) {
                        if (parts[i] == "names") {
                            doc["names"] = true;
                        } else if (i + 1 < partCount) {
                            if (parts[i] == "id") {
                                doc[parts[i]] = parts[i + 1].toInt();
                                i++;
                            } else if (parts[i] == "page" || parts[i] == "pageSize") {
                                doc[parts[i]] = parts[i + 1].toInt();
                                i++;
                            } else {
                                doc[parts[i]] = parts[i + 1];
                                i++;
                            }
                        }
                    }
                } else if (partCount > 2 && parts[0] != "READ") {
                    JsonObject data = doc.createNestedObject("data");
                    Serial.println("Adding data fields:");
                    
                    bool isValid = true;
                    String missingFields = "";
                    String invalidFields = "";
                    
                    if (parts[0] == "CREATE" || parts[0] == "UPDATE") {
                        if (parts[1] == "devices") {
                            const char* requiredFields[] = {"name", "modbus_type", "refresh_rate"};
                            const int numRequired = 3;
                            
                            bool isTCP = false;
                            bool isRTU = false;
                            
                            for (int i = 2; i < partCount; i += 2) {
                                if (i + 1 < partCount && parts[i] == "modbus_type") {
                                    if (parts[i + 1] == "TCP") isTCP = true;
                                    if (parts[i + 1] == "RTU") isRTU = true;
                                    break;
                                }
                            }
                            
                            for (int i = 0; i < numRequired; i++) {
                                bool found = false;
                                for (int j = 2; j < partCount; j += 2) {
                                    if (j + 1 < partCount && parts[j] == requiredFields[i]) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    isValid = false;
                                    if (missingFields.length() > 0) missingFields += ", ";
                                    missingFields += requiredFields[i];
                                }
                            }
                            
                            if (isTCP) {
                                const char* tcpFields[] = {"ip_address", "port", "connection_timeout"};
                                for (const char* field : tcpFields) {
                                    bool found = false;
                                    for (int j = 2; j < partCount; j += 2) {
                                        if (j + 1 < partCount && parts[j] == field) {
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        isValid = false;
                                        if (missingFields.length() > 0) missingFields += ", ";
                                        missingFields += field;
                                    }
                                }
                            } else if (isRTU) {
                                const char* rtuFields[] = {"baudrate", "parity", "data_bits", "stop_bits"};
                                for (const char* field : rtuFields) {
                                    bool found = false;
                                    for (int j = 2; j < partCount; j += 2) {
                                        if (j + 1 < partCount && parts[j] == field) {
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        isValid = false;
                                        if (missingFields.length() > 0) missingFields += ", ";
                                        missingFields += field;
                                    }
                                }
                            }
                        } else if (parts[1] == "config" && parts[0] == "UPDATE") {
                            const char* requiredFields[] = {
                                "communication_type", "communication_mode", "communication_ip", 
                                "communication_mac", "communication_ssid", "communication_pass",
                                "protocol_type", "protocol_server", "protocol_port", 
                                "protocol_topic", "protocol_clientid", "protocol_qos",
                                "interval_time", "interval_type", "auth_type", 
                                "auth_username", "auth_password"
                            };
                            const int numRequired = 17;
                            
                            for (int i = 0; i < numRequired; i++) {
                                bool found = false;
                                for (int j = 2; j < partCount; j += 2) {
                                    if (j + 1 < partCount && parts[j] == requiredFields[i]) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    isValid = false;
                                    if (missingFields.length() > 0) missingFields += ", ";
                                    missingFields += requiredFields[i];
                                }
                            }
                        } else if (parts[1] == "logging_config" && parts[0] == "UPDATE") {
                            const char* requiredFields[] = {"retention", "interval"};
                            const int numRequired = 2;
                            
                            for (int i = 0; i < numRequired; i++) {
                                bool found = false;
                                for (int j = 2; j < partCount; j += 2) {
                                    if (j + 1 < partCount && parts[j] == requiredFields[i]) {
                                        found = true;
                                        if (parts[j] == "retention" && 
                                            !(parts[j + 1] == "1w" || parts[j + 1] == "1m" || parts[j + 1] == "3m")) {
                                            isValid = false;
                                            if (invalidFields.length() > 0) invalidFields += ", ";
                                            invalidFields += "retention (must be 1w, 1m, or 3m)";
                                        }
                                        if (parts[j] == "interval" && 
                                            !(parts[j + 1] == "5m" || parts[j + 1] == "10m" || parts[j + 1] == "30m")) {
                                            isValid = false;
                                            if (invalidFields.length() > 0) invalidFields += ", ";
                                            invalidFields += "interval (must be 5m, 10m, or 30m)";
                                        }
                                        break;
                                    }
                                }
                                if (!found) {
                                    isValid = false;
                                    if (missingFields.length() > 0) missingFields += ", ";
                                    missingFields += requiredFields[i];
                                }
                            }
                        } else if (parts[1] == "clist" || parts[1] == "lglist") {
                            JsonArray dataArray = data.createNestedArray("items");
                            String itemsStr = parts[2];
                            int itemStart = 0;
                            int commaPos;
                            while ((commaPos = itemsStr.indexOf(',', itemStart)) != -1) {
                                String item = itemsStr.substring(itemStart, commaPos);
                                item.trim();
                                if (item.length() > 0) {
                                    dataArray.add(item);
                                }
                                itemStart = commaPos + 1;
                            }
                            String lastItem = itemsStr.substring(itemStart);
                            lastItem.trim();
                            if (lastItem.length() > 0) {
                                dataArray.add(lastItem);
                            }
                        }
                    }
                    
                    if (!isValid) {
                        String response = "{\"success\":false,\"error\":\"";
                        if (missingFields.length() > 0) {
                            response += "Missing required fields: " + missingFields;
                        }
                        if (missingFields.length() > 0 && invalidFields.length() > 0) {
                            response += "; ";
                        }
                        if (invalidFields.length() > 0) {
                            response += "Invalid fields: " + invalidFields;
                        }
                        response += "\"}";
                        sendResponse(response);
                        return;
                    }
                    
                    if (parts[1] == "config") {
                        JsonObject communication = data.createNestedObject("communication");
                        JsonObject protocol = data.createNestedObject("protocol");
                        JsonObject interval = data.createNestedObject("interval");
                        JsonObject auth = data.createNestedObject("auth");
                        
                        for (int i = 2; i < partCount; i += 2) {
                            if (i + 1 < partCount) {
                                Serial.print("  ");
                                Serial.print(parts[i]);
                                Serial.print(" = ");
                                Serial.println(parts[i + 1]);
                                
                                if (parts[i] == "communication_type") communication["type"] = parts[i + 1];
                                else if (parts[i] == "communication_mode") communication["mode"] = parts[i + 1];
                                else if (parts[i] == "communication_ip") communication["ip"] = parts[i + 1];
                                else if (parts[i] == "communication_mac") communication["mac"] = parts[i + 1];
                                else if (parts[i] == "communication_ssid") communication["ssid"] = parts[i + 1];
                                else if (parts[i] == "communication_pass") communication["pass"] = parts[i + 1];
                                else if (parts[i] == "protocol_type") protocol["type"] = parts[i + 1];
                                else if (parts[i] == "protocol_server") protocol["server"] = parts[i + 1];
                                else if (parts[i] == "protocol_port") protocol["port"] = parts[i + 1].toInt();
                                else if (parts[i] == "protocol_topic") protocol["topic"] = parts[i + 1];
                                else if (parts[i] == "protocol_clientid") protocol["clientid"] = parts[i + 1];
                                else if (parts[i] == "protocol_qos") protocol["qos"] = parts[i + 1].toInt();
                                else if (parts[i] == "interval_time") interval["time"] = parts[i + 1].toInt();
                                else if (parts[i] == "interval_type") interval["type"] = parts[i + 1];
                                else if (parts[i] == "auth_type") auth["type"] = parts[i + 1];
                                else if (parts[i] == "auth_username") auth["username"] = parts[i + 1];
                                else if (parts[i] == "auth_password") auth["password"] = parts[i + 1];
                            }
                        }
                    } else if (parts[1] == "logging_config") {
                        for (int i = 2; i < partCount; i += 2) {
                            if (i + 1 < partCount) {
                                Serial.print("  ");
                                Serial.print(parts[i]);
                                Serial.print(" = ");
                                Serial.println(parts[i + 1]);
                                data[parts[i]] = parts[i + 1];
                            }
                        }
                    } else {
                        for (int i = 2; i < partCount; i += 2) {
                            if (i + 1 < partCount) {
                                Serial.print("  ");
                                Serial.print(parts[i]);
                                Serial.print(" = ");
                                Serial.println(parts[i + 1]);
                                if (parts[i] == "id") {
                                    data[parts[i]] = parts[i + 1].toInt();
                                } else if (parts[i] == "refresh_rate" || parts[i] == "baudrate" || 
                                          parts[i] == "port" || parts[i] == "data_bits" || 
                                          parts[i] == "stop_bits" || parts[i] == "connection_timeout") {
                                    data[parts[i]] = parts[i + 1].toInt();
                                    Serial.print("  Converted to number: ");
                                    Serial.println(parts[i + 1].toInt());
                                } else {
                                    data[parts[i]] = parts[i + 1];
                                }
                            }
                        }
                    }
                }
                
                String jsonStr;
                serializeJson(doc, jsonStr);
                Serial.print("Generated JSON: ");
                Serial.println(jsonStr);
                queueCommand(jsonStr);
            } else {
                Serial.println("Error: Invalid command format (need at least action and dataset)");
                sendResponse("Error: Invalid command format");
            }
        }
    }

public:
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue();
        Serial.print("Received data: ");
        Serial.println(value);

        if (value.length() > 0) {
            if (value.startsWith("READ|") || value.startsWith("CREATE|") || 
                value.startsWith("UPDATE|") || value.startsWith("DELETE|")) {
                if (receivedData.length() > 0) {
                    Serial.println("Discarding incomplete data in buffer: " + receivedData);
                    receivedData = "";
                }
            }

            receivedData += value;
            lastReceived = millis();

            Serial.print("Current buffer: ");
            Serial.println(receivedData);

            processBuffer();
        }
    }
};

// Function to send response via BLE
inline void sendResponse(String response) {
  int length = response.length();
  const int maxPacketSize = 20;
  const int maxHeaderSize = 7;
  const int maxPayloadSize = maxPacketSize - maxHeaderSize;

  int numPackets = (length + maxPayloadSize - 1) / maxPayloadSize;

  Serial.print("Sending response, length: ");
  Serial.print(length);
  Serial.print(", packets: ");
  Serial.println(numPackets);

  for (int i = 0; i < numPackets; i++) {
    int start = i * maxPayloadSize;
    int packetPayloadSize = min(maxPayloadSize, length - start);
    String packet = response.substring(start, start + packetPayloadSize);

    String header = String("P") + i + "/" + (numPackets - 1) + ":";
    String packetWithHeader = header + packet;

    if (packetWithHeader.length() > maxPacketSize) {
      Serial.println("Error: Packet size exceeds limit after header addition");
      packetPayloadSize = maxPacketSize - header.length();
      packet = response.substring(start, start + packetPayloadSize);
      packetWithHeader = header + packet;
    }

    pCharacteristic->setValue(packetWithHeader.c_str());
    pCharacteristic->notify();
    Serial.print("Sent packet ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(packetWithHeader);
    delay(20);
  }
}

#endif // BLE_HANDLERS_H