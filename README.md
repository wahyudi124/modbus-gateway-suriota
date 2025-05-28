
# Suriota BLE Configuration Interface (Suriota Gateway)

This document describes the BLE interface for configuring the Suriota device. The device exposes a BLE service that allows CRUD (Create, Read, Update, Delete) operations on various configuration datasets.

## BLE Service Information

- **Service UUID**: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **Characteristic UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Device Name**: `ESP32_Modbus_Server`

## Command Format

Commands are sent to the device using the following format:

```
ACTION|DATASET|param1:value1|param2:value2|...|paramN:valueN#
```

Where:
- `ACTION` is one of: `READ`, `CREATE`, `UPDATE`, or `DELETE`
- `DATASET` is one of: `devices`, `modbus`, `config`, `logging_config`, `clist`, or `lglist`
- Parameters are key-value pairs separated by a colon `:`
- The entire command ends with a hash `#` character

## Available Datasets

### 1. Devices (`devices`)

Manages device configurations.

**Required fields for CREATE/UPDATE**:
- `name`: Device name
- `modbus_type`: Type of Modbus connection (`TCP` or `RTU`)
- `refresh_rate`: Refresh rate in milliseconds

**Additional required fields for TCP devices**:
- `ip_address`: IP address of the device
- `port`: Port number
- `connection_timeout`: Connection timeout in milliseconds

**Additional required fields for RTU devices**:
- `baudrate`: Baud rate
- `parity`: Parity setting
- `data_bits`: Number of data bits
- `stop_bits`: Number of stop bits

### 2. Modbus Configuration (`modbus`)

Manages Modbus register configurations.

### 3. Communication Configuration (`config`)

Manages general communication settings.

**Required fields for UPDATE**:
- `communication_type`, `communication_mode`, `communication_ip`, `communication_mac`, `communication_ssid`, `communication_pass`
- `protocol_type`, `protocol_server`, `protocol_port`, `protocol_topic`, `protocol_clientid`, `protocol_qos`
- `interval_time`, `interval_type`
- `auth_type`, `auth_username`, `auth_password`

### 4. Logging Configuration (`logging_config`)

Manages logging settings.

**Required fields for UPDATE**:
- `retention`: Data retention period (valid values: `1w`, `1m`, or `3m`)
- `interval`: Logging interval (valid values: `5m`, `10m`, or `30m`)

### 5. Custom Lists (`clist` and `lglist`)

Manages custom lists of items.

## Command Examples

### Reading Data

1. Read all devices (paginated):
   ```
   READ|devices|page:1|pageSize:10#
   ```

2. Read a specific device by ID:
   ```
   READ|devices|id:1#
   ```

3. Read all device names:
   ```
   READ|devices|names#
   ```

4. Read communication configuration:
   ```
   READ|config#
   ```

5. Read logging configuration:
   ```
   READ|logging_config#
   ```

### Creating Data

1. Create a new TCP device:
   ```
   CREATE|devices|name:Device1|modbus_type:TCP|refresh_rate:1000|ip_address:192.168.1.100|port:502|connection_timeout:5000#
   ```

2. Create a new RTU device:
   ```
   CREATE|devices|name:Device2|modbus_type:RTU|refresh_rate:1000|baudrate:9600|parity:N|data_bits:8|stop_bits:1#
   ```

### Updating Data

1. Update a device:
   ```
   UPDATE|devices|id:1|name:UpdatedDevice|modbus_type:TCP|refresh_rate:2000|ip_address:192.168.1.101|port:502|connection_timeout:5000#
   ```

2. Update communication configuration:
   ```
   UPDATE|config|communication_type:ETH|communication_mode:A|communication_ip:192.168.0.2|communication_mac:20202020|communication_ssid:TEST|communication_pass:12345678|protocol_type:MQTT|protocol_server:mqtt.example.com|protocol_port:1883|protocol_topic:/data|protocol_clientid:client1|protocol_qos:1|interval_time:30|interval_type:s|auth_type:BASIC|auth_username:user|auth_password:pass#
   ```

3. Update logging configuration:
   ```
   UPDATE|logging_config|retention:1m|interval:10m#
   ```

4. Update a custom list:
   ```
   UPDATE|clist|items:item1,item2,item3#
   ```

### Deleting Data

1. Delete a device:
   ```
   DELETE|devices|id:1#
   ```

## Response Format

Responses are sent back through BLE notifications in the following format:

For single packets:
```
P0/0:response_data
```

For multiple packets:
```
P0/N:first_part_of_response
P1/N:second_part_of_response
...
PN/N:last_part_of_response
```

Where:
- `P` indicates a packet
- The first number is the packet index (starting from 0)
- `N` is the total number of packets minus 1
- The colon `:` separates the header from the payload

## Error Handling

The device will respond with error messages in the following cases:
- Invalid command format
- Missing required fields
- Invalid field values
- Record not found
- File not found
- Error parsing JSON

Example error response:
```
{"success":false,"error":"Missing required fields: name, modbus_type, refresh_rate"}
```

## Caching

The device implements caching for configuration files to improve performance. The cache is automatically invalidated when files are modified.


# FreeRTOS Implementation for Suriota BLE Configuration

This document explains the FreeRTOS implementation in the Suriota BLE Configuration project.

## Overview

The project has been refactored to use FreeRTOS for better task management, improved responsiveness, and more efficient resource utilization. The implementation uses multiple tasks, a command queue, and a mutex for file access synchronization.

## FreeRTOS Components

### Tasks

1. **BLE Task (`bleTask`)**: 
   - Runs on Core 1 (Arduino loop core)
   - Handles BLE initialization and communication
   - Processes incoming BLE commands and queues them for processing
   - Priority: 1

2. **File Task (`fileTask`)**: 
   - Runs on Core 0 (free core)
   - Processes commands from the queue
   - Handles all file operations with mutex protection
   - Priority: 2 (higher than BLE task)

### Synchronization Primitives

1. **Command Queue (`commandQueue`)**:
   - Size: 10 commands
   - Used to pass commands from the BLE task to the File task
   - Decouples command reception from command processing

2. **File Mutex (`fileMutex`)**:
   - Protects file operations from concurrent access
   - Ensures data integrity when reading/writing configuration files

## Command Flow

1. BLE client sends a command to the device
2. `MyCharacteristicCallbacks::onWrite` receives the command
3. Command is parsed and validated in `processBuffer()`
4. Valid command is converted to JSON and sent to `queueCommand()`
5. `queueCommand()` adds the command to the command queue
6. `fileTask` retrieves the command from the queue
7. `fileTask` takes the file mutex before processing the command
8. Command is processed by `processCommand()`
9. Response is sent back to the BLE client
10. File mutex is released

## Benefits

1. **Improved Responsiveness**: BLE communication is not blocked by file operations
2. **Better Resource Utilization**: Tasks run on separate cores, utilizing the dual-core ESP32
3. **Enhanced Stability**: Mutex protection prevents file corruption from concurrent access
4. **Scalability**: Additional tasks can be added for future functionality

## File Structure

- **Suriota2105205od.ino**: Main file with setup() and loop()
- **FreeRTOSTasks.h**: Contains task definitions and implementations
- **BLEHandlers.h**: Modified to use queueCommand() instead of direct processing
- **CommandHandlers.h**: Command processing functions
- **FileOperations.h**: File operations with mutex protection
- **Cache.h**: Cache management functions
- **Constants.h**: Project constants and definitions

## Usage Notes

- The main loop is now empty as all functionality is handled by FreeRTOS tasks
- File operations should only be performed in the file task to maintain synchronization
- BLE responses are still sent directly from any task for immediate feedback
