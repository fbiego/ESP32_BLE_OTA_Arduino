/* Copyright 2022 Vincent Stragier */
#include "main.hpp"

///////////////////
///    Setup    ///
///////////////////
void setup() { ota_dfu_ble.begin("Test OTA DFU"); }

//////////////////
///    Loop    ///
//////////////////
void loop() {
  // Kill the holly loop()
  // Delete the task, comment if you want to keep the loop()
  vTaskDelete(NULL);
}

///////////////////////
///    Functions    ///
///////////////////////
