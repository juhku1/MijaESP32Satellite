# ESP32-C3 Support Branch

This branch contains configuration for ESP32-C3 DevKit (2MB Flash).

kill 475888 && sleep 1 && platformio device monitor --baud 115200

## Hardware
- **Board:** ESP32-C3-DevKitM-1
- **Flash:** 2MB
- **RAM:** 320KB
- **CPU:** RISC-V 160MHz

## Configuration
- **Environment:** `esp32-c3-devkitm-1`
- **Partitions:** 2MB flash partition table
- **Bluetooth:** NimBLE enabled
- **WiFi:** Enabled

## Build & Upload
```bash
platformio run --environment esp32-c3-devkitm-1 --target upload
```

## Status
⚠️ **WORK IN PROGRESS** - Code currently crashes on boot. Debugging needed.

## Known Issues
- Device resets continuously after boot
- No serial output visible
- Possible memory/stack issue

## Next Steps
1. Add debug output to identify crash location
2. Check memory usage and optimize if needed
3. Test minimal code without BLE/WiFi
4. Investigate NimBLE compatibility with ESP32-C3
