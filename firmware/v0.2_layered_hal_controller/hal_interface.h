#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LAYER 3 CONTRACT: HARDWARE ABSTRACTION LAYER (HAL)
// Hardware-independent interface for GPIO, I2C, Addressable LEDs, and Time.
// Application and Driver layers include ONLY this header, never chip headers!
// ============================================================================

typedef enum {
  kHalPinEncA = 0,
  kHalPinEncB,
  kHalPinEncSw,
  kHalPinFaultBtn,
  kHalPinDbgDisplay,
  kHalPinDbgLed,
  kHalPinDbgInput
} HalGpioPin;

typedef enum {
  kHalGpioLow  = 0,
  kHalGpioHigh = 1
} HalGpioLevel;

void HalInit(void);

HalGpioLevel HalGpioGet(HalGpioPin pin);
void HalGpioSet(HalGpioPin pin, HalGpioLevel level);

bool HalI2cWrite(uint8_t addr_7bit, const uint8_t *data, size_t len);

void HalWs2812Write(const uint8_t *grb_data, size_t num_pixels);

uint32_t HalTimeMsGet(void);
void HalDelayMs(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // HAL_INTERFACE_H

