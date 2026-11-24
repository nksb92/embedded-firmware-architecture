#ifndef DRV_LED_H
#define DRV_LED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LAYER 2 (DRIVER): ADDRESSABLE RGB LED DRIVER
// Encapsulates internal pixel buffer and state inside static file scope.
// Exposes validated API: DrvLedBrightnessSet clamps strictly to 0..100%.
// ============================================================================

#define DRV_LED_NUM_PIXELS 8

void DrvLedInit(void);

// Validated API: Fixes the 0% -> 246% underflow bug permanently
void DrvLedBrightnessSet(int16_t percent);
int16_t DrvLedBrightnessGet(void);

// Renders the animation frame scaled by brightness; dark if is_output_on == false
void DrvLedFrameRender(bool is_output_on);

#ifdef __cplusplus
}
#endif

#endif // DRV_LED_H

