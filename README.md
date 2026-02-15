# BLE Satellite (ESP32-C3)

pio device monitor -b 115200

ESP32-C3 BLE satellite device that scans for BLE advertisements and forwards data to a master hub via HTTP.

## Hardware
- **Board**: ESP32-C3-DevKitM-1
- **Chip**: ESP32-C3 (RISC-V, WiFi + BLE)
- **Flash**: 2MB
- **USB**: Native USB-Serial/JTAG

## Features
- **BLE Scanner**: Active scanning for BLE advertisements with device names
- **Auto-Discovery**: Automatically finds master hub IP via UDP broadcasts
- **HTTP Forwarding**: Sends BLE data to master via HTTP POST
- **Lightweight**: Optimized for memory-constrained device
- **Resilient**: Handles network changes, reconnects automatically

## Network Configuration
- **WiFi**: Connects to configured SSID
- **Master Discovery**: 
  - Listens for UDP broadcasts on port 19798
  - Message format: `SATMASTER <IP> <port>`
  - Automatically updates master URL when discovered
- **Fallback**: Uses default master IP if discovery fails

## BLE Scanning
- **Mode**: Active scan (requests device names)
- **Filtering**: Only forwards advertisements with manufacturer data
- **Payload**: Includes MAC address, RSSI, manufacturer data, and device name (if available)

## Data Format
POSTs JSON array to master's `/api/satellite-data` endpoint:
```json
[
  {
    "mac": "AA:BB:CC:DD:EE:FF",
    "rssi": -65,
    "mfg_data": "0100AABBCCDD",
    "name": "DeviceName"
  }
]
```

## Building & Flashing

### Prerequisites
```bash
pip install platformio
```

### Build
```bash
platformio run --environment esp32-c3-devkitm-1
```

### Upload
```bash
platformio run --target upload --environment esp32-c3-devkitm-1 --upload-port /dev/ttyACM0
```

### Monitor
```bash
platformio device monitor -p /dev/ttyACM0 -b 115200
```

## Configuration
WiFi credentials and default master URL are configured at compile time:
```c
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"
#define DEFAULT_SERVER_URL "http://192.168.1.100/api/satellite-data"
```

Note: The default master URL is only used as a fallback. The satellite will automatically discover and use the master's actual IP via UDP broadcasts.

## Discovery Process
1. Satellite boots and connects to WiFi
2. Starts listening for UDP broadcasts on port 19798
3. When master broadcast is received (`SATMASTER <IP> <port>`):
   - Parses master IP and port
   - Updates server URL to `http://<IP>:<port>/api/satellite-data`
   - Logs the change
4. Begins forwarding BLE data to discovered master

## Related Projects
- [SuperMiniProjekti](../SuperMiniProjekti) - ESP32-S3 master hub that receives data from satellites

## License
[Your License Here]
