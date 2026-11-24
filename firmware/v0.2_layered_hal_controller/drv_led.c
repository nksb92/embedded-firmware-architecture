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
} LedDriverCtx;

static LedDriverCtx led_ctx;

static void ColorWheelGet(uint8_t pos, uint8_t *r, uint8_t *g, uint8_t *b) {
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

void DrvLedInit(void) {
  led_ctx.brightness_pct = 50;
  led_ctx.anim_step = 0;
  for (size_t i = 0; i < sizeof(led_ctx.pixel_grb); i++) {
    led_ctx.pixel_grb[i] = 0;
  }
  HalWs2812Write(led_ctx.pixel_grb, DRV_LED_NUM_PIXELS);
}

void DrvLedBrightnessSet(int16_t percent) {
  // VALIDATED MODULE BOUNDARY:
  // Strictly clamps inputs to 0..100%, preventing integer underflow bugs!
  if (percent < 0) {
    led_ctx.brightness_pct = 0;
  } else if (percent > 100) {
    led_ctx.brightness_pct = 100;
  } else {
    led_ctx.brightness_pct = percent;
  }
}

int16_t DrvLedBrightnessGet(void) {
  return led_ctx.brightness_pct;
}

void DrvLedFrameRender(bool is_output_on) {
  HalGpioSet(kHalPinDbgLed, kHalGpioHigh);

  if (!is_output_on || led_ctx.brightness_pct == 0) {
    for (size_t i = 0; i < sizeof(led_ctx.pixel_grb); i++) {
      led_ctx.pixel_grb[i] = 0;
    }
  } else {
    // Scale colors by clamped brightness percentage
    for (size_t i = 0; i < DRV_LED_NUM_PIXELS; i++) {
      uint8_t r, g, b;
      ColorWheelGet(((i * 256 / DRV_LED_NUM_PIXELS) + led_ctx.anim_step) & 255, &r, &g, &b);
      led_ctx.pixel_grb[i * 3 + 0] = (uint8_t)((uint32_t)g * led_ctx.brightness_pct / 100);
      led_ctx.pixel_grb[i * 3 + 1] = (uint8_t)((uint32_t)r * led_ctx.brightness_pct / 100);
      led_ctx.pixel_grb[i * 3 + 2] = (uint8_t)((uint32_t)b * led_ctx.brightness_pct / 100);
    }
    led_ctx.anim_step += 4;
  }

  HalWs2812Write(led_ctx.pixel_grb, DRV_LED_NUM_PIXELS);
  HalGpioSet(kHalPinDbgLed, kHalGpioLow);
}

