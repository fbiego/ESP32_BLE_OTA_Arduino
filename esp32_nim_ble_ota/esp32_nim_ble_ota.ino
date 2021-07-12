/*
   MIT License
   Copyright (c) 2021 Felix Biego
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include <Update.h>
#include "FS.h"
#include "FFat.h"
#include "SPIFFS.h"
#include <NimBLEDevice.h>
//#include <SD.h>             // For OTA with SD Card

#define BUILTINLED 2
#define FORMAT_SPIFFS_IF_FAILED true
#define FORMAT_FFAT_IF_FAILED true

#define USE_SPIFFS  //comment to use FFat

#ifdef USE_SPIFFS
#define FLASH SPIFFS
#define FASTMODE false    //SPIFFS write is slow
#else
#define FLASH FFat
#define FASTMODE true    //FFat is faster
#endif

#define NORMAL_MODE   0   // normal
#define UPDATE_MODE   1   // receiving firmware
#define OTA_MODE      2   // installing firmware

uint8_t updater[16384];
uint8_t updater2[16384];

#define SERVICE_UUID              "fb1e4001-54ae-4a28-9f74-dfccb248601d"
#define CHARACTERISTIC_UUID_RX    "fb1e4002-54ae-4a28-9f74-dfccb248601d"
#define CHARACTERISTIC_UUID_TX    "fb1e4003-54ae-4a28-9f74-dfccb248601d"

static BLECharacteristic* pCharacteristicTX;
static BLECharacteristic* pCharacteristicRX;

static bool deviceConnected = false, sendMode = false;
static bool writeFile = false, request = false;
static int writeLen = 0, writeLen2 = 0;
static bool current = true;
static int parts = 0, next = 0, cur = 0, MTU = 0;
static int MODE = NORMAL_MODE;

static void rebootEspWithReason(String reason) {
  Serial.println(reason);
  delay(1000);
  ESP.restart();
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
      deviceConnected = true;

    }
    void onDisconnect(BLEServer* pServer) {
      Serial.println("disconnected");
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {

    //    void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
    //      Serial.print("Status ");
    //      Serial.print(s);
    //      Serial.print(" on characteristic ");
    //      Serial.print(pCharacteristic->getUUID().toString().c_str());
    //      Serial.print(" with code ");
    //      Serial.println(code);
    //    }

    void onRead(BLECharacteristic* pCharacteristic) {
      Serial.print(pCharacteristic->getUUID().toString().c_str());
      Serial.print(": onRead(), value: ");
      Serial.println(pCharacteristic->getValue().c_str());
    };

    void onNotify(BLECharacteristic *pCharacteristic) {
      //uint8_t* pData;
      std::string pData = pCharacteristic->getValue();
      int len = pData.length();
      //        Serial.print("Notify callback for characteristic ");
      //        Serial.print(pCharacteristic->getUUID().toString().c_str());
      //        Serial.print(" of data length ");
      //        Serial.println(len);
      Serial.print("TX  ");
      for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", pData[i]);
      }
      Serial.println();
    }

    void onWrite(BLECharacteristic *pCharacteristic) {
      //uint8_t* pData;
      std::string pData = pCharacteristic->getValue();
      int len = pData.length();
      // Serial.print("Write callback for characteristic ");
      // Serial.print(pCharacteristic->getUUID().toString().c_str());
      // Serial.print(" of data length ");
      // Serial.println(len);
      // Serial.print("RX  ");
      // for (int i = 0; i < len; i++) {         // leave this commented
      //   Serial.printf("%02X ", pData[i]);
      // }
      // Serial.println();

      if (pData[0] == 0xFB) {
        int pos = pData[1];
        for (int x = 0; x < len - 2; x++) {
          if (current) {
            updater[(pos * MTU) + x] = pData[x + 2];
          } else {
            updater2[(pos * MTU) + x] = pData[x + 2];
          }
        }

      } else if  (pData[0] == 0xFC) {
        if (current) {
          writeLen = (pData[1] * 256) + pData[2];
        } else {
          writeLen2 = (pData[1] * 256) + pData[2];
        }
        current = !current;
        cur = (pData[3] * 256) + pData[4];
        writeFile = true;
        if (cur < parts - 1) {
          request = !FASTMODE;
        }
      } else if (pData[0] == 0xFD) {
        sendMode = true;
        if (FLASH.exists("/update.bin")) {
          FLASH.remove("/update.bin");
        }
      } else if  (pData[0] == 0xFF) {
        parts = (pData[1] * 256) + pData[2];
        MTU = (pData[3] * 256) + pData[4];
        MODE = UPDATE_MODE;

      }



    }


};

static void writeBinary(fs::FS &fs, const char * path, uint8_t *dat, int len) {

  //Serial.printf("Write binary file %s\r\n", path);

  File file = fs.open(path, FILE_APPEND);

  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  file.write(dat, len);
  file.close();
}

void initBLE() {
  BLEDevice::init("NimBLE OTA");
  BLEDevice::setMTU(517);
  BLEDevice::setPower(ESP_PWR_LVL_P9);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ );
  pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pCharacteristicTX->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  NimBLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();
  //BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE OTA sketch");
  pinMode(BUILTINLED, OUTPUT);

  //SPI.begin(18, 22, 23, 5);       // For OTA with SD Card
  //SD.begin(5);                    // For OTA with SD Card

#ifdef USE_SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
#else
  if (!FFat.begin()) {
    Serial.println("FFat Mount Failed");
    if (FORMAT_FFAT_IF_FAILED) FFat.format();
    return;
  }
#endif


  initBLE();

}

void loop() {

  switch (MODE) {

    case NORMAL_MODE:
      if (deviceConnected) {
        digitalWrite(BUILTINLED, HIGH);
        if (sendMode) {
          uint8_t fMode[] = {0xAA, FASTMODE};
          pCharacteristicTX->setValue(fMode, 2);
          pCharacteristicTX->notify();
          delay(50);
          sendMode = false;
        }

        // your loop code here
      } else {
        digitalWrite(BUILTINLED, LOW);
      }

      // or here

      break;

    case UPDATE_MODE:

      if (request) {
        uint8_t rq[] = {0xF1, (cur + 1) / 256, (cur + 1) % 256};
        pCharacteristicTX->setValue(rq, 3);
        pCharacteristicTX->notify();
        delay(50);
        request = false;
      }

      if (writeFile) {
        if (!current) {
          writeBinary(FLASH, "/update.bin", updater, writeLen);
        } else {
          writeBinary(FLASH, "/update.bin", updater2, writeLen2);
        }
        writeFile = false;
      }

      if (cur + 1 == parts) { // received complete file
        uint8_t com[] = {0xF2, (cur + 1) / 256, (cur + 1) % 256};
        pCharacteristicTX->setValue(com, 3);
        pCharacteristicTX->notify();
        delay(50);
        MODE = OTA_MODE;
      }

      break;

    case OTA_MODE:
      updateFromFS(FLASH);
      break;

  }

}

void sendOtaResult(String result) {
  byte arr[result.length()];
  result.getBytes(arr, result.length());
  pCharacteristicTX->setValue(arr, result.length());
  pCharacteristicTX->notify();
  delay(200);
}


void performUpdate(Stream &updateSource, size_t updateSize) {
  char s1 = 0x0F;
  String result = String(s1);
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    result += "Written : " + String(written) + "/" + String(updateSize) + " [" + String((written / updateSize) * 100) + "%] \n";
    if (Update.end()) {
      Serial.println("OTA done!");
      result += "OTA Done: ";
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting...");
        result += "Success!\n";
      }
      else {
        Serial.println("Update not finished? Something went wrong!");
        result += "Failed!\n";
      }

    }
    else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      result += "Error #: " + String(Update.getError());
    }
  }
  else
  {
    Serial.println("Not enough space to begin OTA");
    result += "Not enough space for OTA";
  }
  if (deviceConnected) {
    sendOtaResult(result);
    delay(5000);
  }
}

void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Serial.println("Trying to start update");
      performUpdate(updateBin, updateSize);
    }
    else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    // when finished remove the binary from spiffs to indicate end of the process
    Serial.println("Removing update file");
    fs.remove("/update.bin");

    rebootEspWithReason("Rebooting to complete OTA update");
  }
  else {
    Serial.println("Could not load update.bin from spiffs root");
  }
}
