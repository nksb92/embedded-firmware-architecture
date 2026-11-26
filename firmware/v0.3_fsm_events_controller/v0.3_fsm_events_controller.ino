// ============================================================================
// Another German Engineer — Embedded Firmware Architecture (Part 2 / Video #2)
// Version 0.3: Finite State Machine & FIFO Event Queue
// Thin entry point delegating directly to application lifecycle functions.
// Note: Layer 2 (Drivers) and Layer 3 (HAL) are 100% unchanged from v0.2.
// ============================================================================

#include "app_controller.h"

void setup() {
  AppInit();
}

void loop() {
  AppUpdate();
}
