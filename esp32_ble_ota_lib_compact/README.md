# Compact version of the BLE OTA example

This directory contains a PlatformIO project which implement a compact version of the OTA. It also provides a library (in the lib folder) that allows to easily port BLE OTA to your own code.

**Note:** this version of the code is not compatible with the mobile app for the moment (the update mechanism is not exactly the same).

Furthermore, to use the FFAT mode you need to use another partition table (which you must configure in `platformio.ini`).
