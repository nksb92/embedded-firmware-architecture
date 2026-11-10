// ============================================================================
// Another German Engineer — Embedded Firmware Architecture (Part 1 / Video #1)
// Version 0.2: Three-Layer Hardware Abstraction Layer & Encapsulated C Modules
// Thin entry point delegating directly to application lifecycle functions.
// ============================================================================

#include "app_controller.h"

void setup() {
  app_init();
}

void loop() {
  app_update();
}

