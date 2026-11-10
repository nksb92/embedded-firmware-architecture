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
// Exposes validated API: drv_led_set_brightness clamps strictly to 0..100%.
// ============================================================================

#define DRV_LED_NUM_PIXELS 8

void drv_led_init(void);

// Validated API: Fixes the 0% -> 246% underflow bug permanently
void drv_led_set_brightness(int16_t percent);
int16_t drv_led_get_brightness(void);

// Renders the animation frame scaled by brightness; dark if is_output_on == false
void drv_led_render_frame(bool is_output_on);

#ifdef __cplusplus
}
#endif

#endif // DRV_LED_H

