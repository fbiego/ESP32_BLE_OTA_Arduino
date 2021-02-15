# ESP32_BLE_OTA_Arduino
OTA update on ESP32 via BLE


## Android
[`Android App`](https://github.com/fbiego/ESP32_BLE_OTA_Android)

## Receive Sequence
1. Receive command to format SPIFFS
2. Receive number of parts and MTU size
3. Receive first part and write data to `updater` array
4. Receive end of first part command, append `updater` array to update.bin file
5. Request the next part (repeats 3, 4 & 5 until all parts have been received)
6. Receive command to trigger restart to apply update
