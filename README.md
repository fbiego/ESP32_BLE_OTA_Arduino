# ESP32_BLE_OTA_Arduino
OTA update on ESP32 via BLE

- 1,005,392 bytes uploaded in 5mins 20sec
- Speed: ~ `3kb/s`

## Android app
[`BLE-OTA-v1.0.apk`](https://github.com/fbiego/ESP32_BLE_OTA_Arduino/raw/main/BLE-OTA-v1.0.apk)

## Video
[`ESP32 OTA via BLE`](https://youtu.be/j4ELTS7QXFM)

## Receive Sequence
1. Receive command to format SPIFFS
2. Receive number of parts and MTU size
3. Receive first part and write data to `updater` array
4. Receive end of first part command, append `updater` array to update.bin file
5. Request the next part (repeats 3, 4 & 5 until all parts have been received)
6. Receive command to trigger restart to apply update
