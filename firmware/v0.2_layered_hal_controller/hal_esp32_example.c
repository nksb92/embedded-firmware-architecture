#include "hal_interface.h"

// ============================================================================
// LAYER 3 (HAL): ESP32 REFERENCE DROP-IN IMPLEMENTATION
// Demonstrates that swapping from RP2040 to ESP32 requires replacing ONLY this file.
// The Application Layer and Drivers remain 100% untouched.
// ============================================================================

#if defined(ESP32)

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP32_I2C_PORT       I2C_NUM_0
#define ESP32_PIN_SDA        21
#define ESP32_PIN_SCL        22
#define ESP32_PIN_ENC_A      32
#define ESP32_PIN_ENC_B      33
#define ESP32_PIN_ENC_SW     25
#define ESP32_PIN_FAULT_BTN  26
#define ESP32_PIN_DBG_DISP   16
#define ESP32_PIN_DBG_LED    17
#define ESP32_PIN_DBG_INPUT  4

static gpio_num_t get_esp32_pin(hal_gpio_pin_t pin) {
  switch (pin) {
    case HAL_PIN_ENC_A:       return (gpio_num_t)ESP32_PIN_ENC_A;
    case HAL_PIN_ENC_B:       return (gpio_num_t)ESP32_PIN_ENC_B;
    case HAL_PIN_ENC_SW:      return (gpio_num_t)ESP32_PIN_ENC_SW;
    case HAL_PIN_FAULT_BTN:   return (gpio_num_t)ESP32_PIN_FAULT_BTN;
    case HAL_PIN_DBG_DISPLAY: return (gpio_num_t)ESP32_PIN_DBG_DISP;
    case HAL_PIN_DBG_LED:     return (gpio_num_t)ESP32_PIN_DBG_LED;
    case HAL_PIN_DBG_INPUT:   return (gpio_num_t)ESP32_PIN_DBG_INPUT;
    default:                  return GPIO_NUM_NC;
  }
}

void hal_init(void) {
  gpio_config_t in_conf = {
    .pin_bit_mask = (1ULL << ESP32_PIN_ENC_A) | (1ULL << ESP32_PIN_ENC_B) |
                    (1ULL << ESP32_PIN_ENC_SW) | (1ULL << ESP32_PIN_FAULT_BTN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&in_conf);

  gpio_config_t out_conf = {
    .pin_bit_mask = (1ULL << ESP32_PIN_DBG_DISP) | (1ULL << ESP32_PIN_DBG_LED) | (1ULL << ESP32_PIN_DBG_INPUT),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&out_conf);

  i2c_config_t i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = ESP32_PIN_SDA,
    .scl_io_num = ESP32_PIN_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000
  };
  i2c_param_config(ESP32_I2C_PORT, &i2c_conf);
  i2c_driver_install(ESP32_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

hal_gpio_level_t hal_gpio_read(hal_gpio_pin_t pin) {
  gpio_num_t p = get_esp32_pin(pin);
  if (p == GPIO_NUM_NC) return HAL_GPIO_LOW;
  return gpio_get_level(p) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
}

void hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_level_t level) {
  gpio_num_t p = get_esp32_pin(pin);
  if (p != GPIO_NUM_NC) {
    gpio_set_level(p, (level == HAL_GPIO_HIGH) ? 1 : 0);
  }
}

bool hal_i2c_write(uint8_t addr_7bit, const uint8_t *data, size_t len) {
  esp_err_t ret = i2c_master_write_to_device(ESP32_I2C_PORT, addr_7bit, data, len, pdMS_TO_TICKS(100));
  return (ret == ESP_OK);
}

void hal_ws2812_write(const uint8_t *grb_data, size_t num_pixels) {
  // ESP32 RMT peripheral driver implementation would go here
  (void)grb_data;
  (void)num_pixels;
}

uint32_t hal_get_time_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void hal_delay_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

#endif // ESP32

