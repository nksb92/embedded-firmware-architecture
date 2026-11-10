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
  HAL_PIN_ENC_A = 0,
  HAL_PIN_ENC_B,
  HAL_PIN_ENC_SW,
  HAL_PIN_FAULT_BTN,
  HAL_PIN_DBG_DISPLAY,
  HAL_PIN_DBG_LED,
  HAL_PIN_DBG_INPUT
} hal_gpio_pin_t;

typedef enum {
  HAL_GPIO_LOW  = 0,
  HAL_GPIO_HIGH = 1
} hal_gpio_level_t;

void hal_init(void);

hal_gpio_level_t hal_gpio_read(hal_gpio_pin_t pin);
void hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_level_t level);

bool hal_i2c_write(uint8_t addr_7bit, const uint8_t *data, size_t len);

void hal_ws2812_write(const uint8_t *grb_data, size_t num_pixels);

uint32_t hal_get_time_ms(void);
void hal_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // HAL_INTERFACE_H

