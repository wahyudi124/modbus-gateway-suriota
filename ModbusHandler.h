#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include "Cache.h"

// Tipe data yang didukung untuk register Modbus
enum ModbusDataType {
  BOOL,
  UINT16,
  INT16,
  UINT32,
  INT32,
  FLOAT32,
  FLOAT64,
  FLOAT
};

// Struktur untuk konfigurasi perangkat
struct DeviceConfig {
  String name;
  String modbus_type;
  uint32_t refresh_rate;
  uint8_t id;
  
  // Untuk Modbus RTU
  uint32_t baudrate;
  String parity;
  uint8_t data_bits;
  uint8_t stop_bits;
  
  // Untuk Modbus TCP
  String ip_address;
  uint16_t port;
  uint32_t connection_timeout;
};

// Struktur untuk konfigurasi register
struct RegisterConfig {
  String name;
  String device_choose;
  ModbusDataType data_type;
  uint16_t address;
  uint8_t function_code;
  uint8_t id;
};

class ModbusHandler {
public:
  ModbusHandler() : devices(nullptr), registers(nullptr), deviceCount(0), registerCount(0) {}
  
  ~ModbusHandler() {
    if (devices) delete[] devices;
    if (registers) delete[] registers;
  }
  
  bool init(const char* devicesPath, const char* modbusConfigPath) {
    // Load konfigurasi dari cache
    if (!loadConfig(devicesPath, modbusConfigPath)) {
      Serial.println("Failed to load configuration from cache!");
      return false;
    }
    
    // Inisialisasi Ethernet untuk koneksi TCP
    byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Ethernet using DHCP");
      return false;
    }
    
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    
    // Inisialisasi Serial2 untuk Modbus RTU
    Serial2.begin(9600, SERIAL_8N1, 15, 16);
    
    return true;
  }
  
  uint8_t getRegisterCount() {
    return registerCount;
  }
  
  String getRegisterName(uint8_t index) {
    if (index < registerCount) {
      return registers[index].name;
    }
    return "";
  }
  
  float readRegister(uint8_t index) {
    if (index >= registerCount) {
      return 0.0;
    }
    
    // Cari device yang sesuai dengan register
    DeviceConfig* dev = nullptr;
    for (uint8_t i = 0; i < deviceCount; i++) {
      if (devices[i].name == registers[index].device_choose) {
        dev = &devices[i];
        break;
      }
    }
    
    if (!dev) {
      Serial.print("Device not found for register: ");
      Serial.println(registers[index].name);
      return 0.0;
    }
    
    return readModbusValue(registers[index], *dev);
  }
  
private:
  DeviceConfig* devices;
  RegisterConfig* registers;
  uint8_t deviceCount;
  uint8_t registerCount;
  
  bool loadConfig(const char* devicesPath, const char* modbusConfigPath) {
    DynamicJsonDocument devicesDoc(4096);
    DynamicJsonDocument configDoc(8192);
    
    // Baca dari cache
    if (!loadFromCache(devicesPath, devicesDoc)) {
      Serial.println("Failed to load devices from cache");
      return false;
    }
    
    if (!loadFromCache(modbusConfigPath, configDoc)) {
      Serial.println("Failed to load modbus config from cache");
      return false;
    }
    
    // Alokasi memori untuk devices dan registers
    deviceCount = devicesDoc.size();
    registerCount = configDoc.size();
    
    devices = new DeviceConfig[deviceCount];
    registers = new RegisterConfig[registerCount];
    
    // Parse devices
    for (uint8_t i = 0; i < deviceCount; i++) {
      JsonObject device = devicesDoc[i];
      devices[i].name = device["name"].as<String>();
      devices[i].modbus_type = device["modbus_type"].as<String>();
      devices[i].refresh_rate = device["refresh_rate"];
      devices[i].id = device["id"];
      
      if (devices[i].modbus_type == "RTU") {
        devices[i].baudrate = device["baudrate"];
        devices[i].parity = device["parity"].as<String>();
        devices[i].data_bits = device["data_bits"];
        devices[i].stop_bits = device["stop_bits"];
      } else if (devices[i].modbus_type == "TCP") {
        devices[i].ip_address = device["ip_address"].as<String>();
        devices[i].port = device["port"];
        devices[i].connection_timeout = device["connection_timeout"];
      }
    }
    
    // Parse registers
    for (uint8_t i = 0; i < registerCount; i++) {
      JsonObject reg = configDoc[i];
      registers[i].name = reg["name"].as<String>();
      registers[i].device_choose = reg["device_choose"].as<String>();
      registers[i].data_type = stringToDataType(reg["data_type"].as<String>());
      registers[i].address = reg["address"].as<String>().toInt();
      registers[i].function_code = reg["function_code"].as<String>().toInt();
      registers[i].id = reg["id"];
    }
    
    return true;
  }
  
  ModbusDataType stringToDataType(const String& dataType) {
    if (dataType == "BOOL") return BOOL;
    if (dataType == "UINT16") return UINT16;
    if (dataType == "INT16") return INT16;
    if (dataType == "UINT32") return UINT32;
    if (dataType == "INT32") return INT32;
    if (dataType == "FLOAT32") return FLOAT32;
    if (dataType == "FLOAT64") return FLOAT64;
    if (dataType == "FLOAT") return FLOAT;
    
    // Default
    return UINT16;
  }
  
  void setupRTU(const DeviceConfig& dev) {
    // Setup Serial2 untuk komunikasi Modbus RTU
    Serial2.flush();
    Serial2.end();
    
    // Setup parity
    if (dev.parity == "none") {
      Serial2.begin(dev.baudrate, SERIAL_8N1);
    } else if (dev.parity == "even") {
      Serial2.begin(dev.baudrate, SERIAL_8E1);
    } else if (dev.parity == "odd") {
      Serial2.begin(dev.baudrate, SERIAL_8O1);
    }
    
    // Tunggu Serial2 siap
    delay(50);
  }
  
  IPAddress setupTCP(const DeviceConfig& dev) {
    // Parse IP address
    IPAddress ip;
    ip.fromString(dev.ip_address);
    return ip;
  }
  
  float readModbusValue(const RegisterConfig& reg, const DeviceConfig& dev) {
    uint16_t result[4] = {0}; // Buffer untuk menyimpan hasil pembacaan (maksimal 4 register untuk FLOAT64)
    bool success = false;
    
    // Setup koneksi Modbus sesuai tipe
    if (dev.modbus_type == "RTU") {
      setupRTU(dev);
      
      // Implementasi sederhana Modbus RTU
      uint8_t txBuffer[8];
      uint8_t rxBuffer[256];
      uint8_t rxLen = 0;
      
      // Buat request Modbus RTU
      txBuffer[0] = dev.id;
      txBuffer[1] = reg.function_code;
      txBuffer[2] = highByte(reg.address);
      txBuffer[3] = lowByte(reg.address);
      
      uint8_t numRegisters = 1;
      if (reg.data_type == UINT32 || reg.data_type == INT32 || reg.data_type == FLOAT32) {
        numRegisters = 2;
      } else if (reg.data_type == FLOAT64) {
        numRegisters = 4;
      }
      
      txBuffer[4] = 0;
      txBuffer[5] = numRegisters;
      
      // Hitung CRC
      uint16_t crc = 0xFFFF;
      for (int i = 0; i < 6; i++) {
        crc ^= txBuffer[i];
        for (int j = 0; j < 8; j++) {
          if (crc & 0x0001) {
            crc >>= 1;
            crc ^= 0xA001;
          } else {
            crc >>= 1;
          }
        }
      }
      
      txBuffer[6] = lowByte(crc);
      txBuffer[7] = highByte(crc);
      
      // Kirim request
      Serial2.flush();
      for (int i = 0; i < 8; i++) {
        Serial2.write(txBuffer[i]);
      }
      
      // Tunggu respons
      unsigned long startTime = millis();
      while ((Serial2.available() < 5) && (millis() - startTime < 1000)) {
        delay(1);
      }
      
      // Baca respons
      if (Serial2.available() > 0) {
        rxLen = 0;
        while (Serial2.available() > 0 && rxLen < 256) {
          rxBuffer[rxLen++] = Serial2.read();
        }
        
        // Verifikasi respons
        if (rxLen >= 5 && rxBuffer[0] == dev.id && rxBuffer[1] == reg.function_code) {
          // Ambil data
          if (reg.function_code == 1 || reg.function_code == 2) {
            // Coils atau Discrete Inputs
            result[0] = rxBuffer[3];
          } else if (reg.function_code == 3 || reg.function_code == 4) {
            // Holding atau Input Registers
            for (uint8_t i = 0; i < numRegisters; i++) {
              result[i] = (rxBuffer[3 + i * 2] << 8) | rxBuffer[4 + i * 2];
            }
          }
          success = true;
        }
      }
    } else if (dev.modbus_type == "TCP") {
      IPAddress ip = setupTCP(dev);
      EthernetClient client;
      
      // Buat koneksi TCP
      if (client.connect(ip, dev.port)) {
        // Implementasi sederhana Modbus TCP
        uint8_t txBuffer[12];
        uint8_t rxBuffer[256];
        uint8_t rxLen = 0;
        
        // Buat request Modbus TCP
        uint16_t transactionId = random(65535);
        txBuffer[0] = highByte(transactionId);
        txBuffer[1] = lowByte(transactionId);
        txBuffer[2] = 0; // Protocol ID (0 for Modbus)
        txBuffer[3] = 0; // Protocol ID (0 for Modbus)
        
        uint8_t numRegisters = 1;
        if (reg.data_type == UINT32 || reg.data_type == INT32 || reg.data_type == FLOAT32) {
          numRegisters = 2;
        } else if (reg.data_type == FLOAT64) {
          numRegisters = 4;
        }
        
        uint8_t len = 6; // Unit ID + Function Code + Address (2) + Quantity (2)
        txBuffer[4] = 0;
        txBuffer[5] = len;
        txBuffer[6] = dev.id; // Unit ID
        txBuffer[7] = reg.function_code;
        txBuffer[8] = highByte(reg.address);
        txBuffer[9] = lowByte(reg.address);
        txBuffer[10] = 0;
        txBuffer[11] = numRegisters;
        
        // Kirim request
        client.write(txBuffer, 12);
        
        // Tunggu respons
        unsigned long startTime = millis();
        while ((!client.available()) && (millis() - startTime < dev.connection_timeout)) {
          delay(1);
        }
        
        // Baca respons
        if (client.available()) {
          rxLen = 0;
          while (client.available() && rxLen < 256) {
            rxBuffer[rxLen++] = client.read();
          }
          
          // Verifikasi respons
          if (rxLen >= 9 && 
              rxBuffer[0] == txBuffer[0] && rxBuffer[1] == txBuffer[1] && // Transaction ID
              rxBuffer[6] == dev.id && rxBuffer[7] == reg.function_code) {
            
            // Ambil data
            if (reg.function_code == 1 || reg.function_code == 2) {
              // Coils atau Discrete Inputs
              result[0] = rxBuffer[9];
            } else if (reg.function_code == 3 || reg.function_code == 4) {
              // Holding atau Input Registers
              for (uint8_t i = 0; i < numRegisters; i++) {
                result[i] = (rxBuffer[9 + i * 2] << 8) | rxBuffer[10 + i * 2];
              }
            }
            success = true;
          }
        }
        
        // Tutup koneksi
        client.stop();
      }
    }
    
    if (!success) {
      Serial.print("Failed to read register: ");
      Serial.println(reg.name);
      return 0.0;
    }
    
    // Konversi hasil sesuai tipe data
    switch (reg.data_type) {
      case BOOL:
        return (float)result[0];
      case UINT16:
        return (float)result[0];
      case INT16:
        return (float)(int16_t)result[0];
      case UINT32: {
        uint32_t val = ((uint32_t)result[0] << 16) | result[1];
        return (float)val;
      }
      case INT32: {
        int32_t val = ((int32_t)result[0] << 16) | result[1];
        return (float)val;
      }
      case FLOAT32: {
        union {
          uint32_t i;
          float f;
        } conv;
        conv.i = ((uint32_t)result[0] << 16) | result[1];
        return conv.f;
      }
      case FLOAT64: {
        union {
          uint64_t i;
          double d;
        } conv;
        conv.i = ((uint64_t)result[0] << 48) | ((uint64_t)result[1] << 32) | ((uint64_t)result[2] << 16) | result[3];
        return (float)conv.d;
      }
      case FLOAT: {
        union {
          uint32_t i;
          float f;
        } conv;
        conv.i = ((uint32_t)result[0] << 16) | result[1];
        return conv.f;
      }
      default:
        return 0.0;
    }
  }
};

#endif // MODBUS_HANDLER_H