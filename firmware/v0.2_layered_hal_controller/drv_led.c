#include "drv_led.h"
#include "hal_interface.h"

// ============================================================================
// LAYER 2: ENCAPSULATED STATIC MODULE STATE
// No extern globals! Module data is strictly private to this compilation unit.
// ============================================================================
typedef struct {
  int16_t brightness_pct;
  uint8_t anim_step;
  uint8_t pixel_grb[DRV_LED_NUM_PIXELS * 3];
} led_driver_ctx_t;

static led_driver_ctx_t s_led_ctx;

static void wheel(uint8_t pos, uint8_t *r, uint8_t *g, uint8_t *b) {
  pos = 255 - pos;
  if (pos < 85) {
    *r = 255 - pos * 3;
    *g = 0;
    *b = pos * 3;
  } else if (pos < 170) {
    pos -= 85;
    *r = 0;
    *g = pos * 3;
    *b = 255 - pos * 3;
  } else {
    pos -= 170;
    *r = pos * 3;
    *g = 255 - pos * 3;
    *b = 0;
  }
}

void drv_led_init(void) {
  s_led_ctx.brightness_pct = 50;
  s_led_ctx.anim_step = 0;
  for (size_t i = 0; i < sizeof(s_led_ctx.pixel_grb); i++) {
    s_led_ctx.pixel_grb[i] = 0;
  }
  hal_ws2812_write(s_led_ctx.pixel_grb, DRV_LED_NUM_PIXELS);
}

void drv_led_set_brightness(int16_t percent) {
  // VALIDATED MODULE BOUNDARY:
  // Strictly clamps inputs to 0..100%, preventing integer underflow bugs!
  if (percent < 0) {
    s_led_ctx.brightness_pct = 0;
  } else if (percent > 100) {
    s_led_ctx.brightness_pct = 100;
  } else {
    s_led_ctx.brightness_pct = percent;
  }
}

int16_t drv_led_get_brightness(void) {
  return s_led_ctx.brightness_pct;
}

void drv_led_render_frame(bool is_output_on) {
  hal_gpio_write(HAL_PIN_DBG_LED, HAL_GPIO_HIGH);

  if (!is_output_on || s_led_ctx.brightness_pct == 0) {
    for (size_t i = 0; i < sizeof(s_led_ctx.pixel_grb); i++) {
      s_led_ctx.pixel_grb[i] = 0;
    }
  } else {
    // Scale colors by clamped brightness percentage
    for (size_t i = 0; i < DRV_LED_NUM_PIXELS; i++) {
      uint8_t r, g, b;
      wheel(((i * 256 / DRV_LED_NUM_PIXELS) + s_led_ctx.anim_step) & 255, &r, &g, &b);
      s_led_ctx.pixel_grb[i * 3 + 0] = (uint8_t)((uint32_t)g * s_led_ctx.brightness_pct / 100);
      s_led_ctx.pixel_grb[i * 3 + 1] = (uint8_t)((uint32_t)r * s_led_ctx.brightness_pct / 100);
      s_led_ctx.pixel_grb[i * 3 + 2] = (uint8_t)((uint32_t)b * s_led_ctx.brightness_pct / 100);
    }
    s_led_ctx.anim_step += 4;
  }

  hal_ws2812_write(s_led_ctx.pixel_grb, DRV_LED_NUM_PIXELS);
  hal_gpio_write(HAL_PIN_DBG_LED, HAL_GPIO_LOW);
}

