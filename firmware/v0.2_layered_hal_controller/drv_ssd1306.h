#ifndef DRV_SSD1306_H
#define DRV_SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LAYER 2 (DRIVER): HARDWARE-AGNOSTIC SSD1306 OLED DRIVER
// Uses HalI2cWrite to communicate with the display.
// Zero processor-specific register names or Arduino Wire references.
// ============================================================================

void DrvSsd1306Init(void);

// Renders one of three screens:
// screen_mode: 0 = Normal Status, 1 = Edit Brightness, 2 = Hardware Fault
void DrvSsd1306Render(uint8_t screen_mode, bool is_output_on, int16_t brightness, bool is_locked);

#ifdef __cplusplus
}
#endif

#endif // DRV_SSD1306_H

