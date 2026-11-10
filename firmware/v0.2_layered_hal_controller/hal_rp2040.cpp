#include "hal_interface.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define PIN_SDA          4
#define PIN_SCL          5
#define PIN_ENC_A        10
#define PIN_ENC_B        11
#define PIN_ENC_SW       12
#define PIN_FAULT_BTN    13
#define PIN_WS2812       15
#define PIN_DBG_DISPLAY  18
#define PIN_DBG_LED      19
#define PIN_DBG_INPUT    20

#define MAX_WS2812_LEDS  8

static Adafruit_NeoPixel s_strip(MAX_WS2812_LEDS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

static int get_rp2040_pin(hal_gpio_pin_t pin) {
  switch (pin) {
    case HAL_PIN_ENC_A:       return PIN_ENC_A;
    case HAL_PIN_ENC_B:       return PIN_ENC_B;
    case HAL_PIN_ENC_SW:      return PIN_ENC_SW;
    case HAL_PIN_FAULT_BTN:   return PIN_FAULT_BTN;
    case HAL_PIN_DBG_DISPLAY: return PIN_DBG_DISPLAY;
    case HAL_PIN_DBG_LED:     return PIN_DBG_LED;
    case HAL_PIN_DBG_INPUT:   return PIN_DBG_INPUT;
    default:                  return -1;
  }
}

extern "C" {

void hal_init(void) {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_FAULT_BTN, INPUT_PULLUP);

  pinMode(PIN_DBG_DISPLAY, OUTPUT);
  pinMode(PIN_DBG_LED, OUTPUT);
  pinMode(PIN_DBG_INPUT, OUTPUT);

  digitalWrite(PIN_DBG_DISPLAY, LOW);
  digitalWrite(PIN_DBG_LED, LOW);
  digitalWrite(PIN_DBG_INPUT, LOW);

  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setClock(100000);

  s_strip.begin();
  s_strip.show();
}

hal_gpio_level_t hal_gpio_read(hal_gpio_pin_t pin) {
  int p = get_rp2040_pin(pin);
  if (p < 0) return HAL_GPIO_LOW;
  return (digitalRead(p) == HIGH) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
}

void hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_level_t level) {
  int p = get_rp2040_pin(pin);
  if (p >= 0) {
    digitalWrite(p, (level == HAL_GPIO_HIGH) ? HIGH : LOW);
  }
}

bool hal_i2c_write(uint8_t addr_7bit, const uint8_t *data, size_t len) {
  Wire.beginTransmission(addr_7bit);
  size_t written = Wire.write(data, len);
  return (Wire.endTransmission() == 0) && (written == len);
}

void hal_ws2812_write(const uint8_t *grb_data, size_t num_pixels) {
  if (num_pixels > MAX_WS2812_LEDS) num_pixels = MAX_WS2812_LEDS;
  for (size_t i = 0; i < num_pixels; i++) {
    uint8_t g = grb_data[i * 3 + 0];
    uint8_t r = grb_data[i * 3 + 1];
    uint8_t b = grb_data[i * 3 + 2];
    s_strip.setPixelColor(i, s_strip.Color(r, g, b));
  }
  s_strip.show();
}

uint32_t hal_get_time_ms(void) {
  return millis();
}

void hal_delay_ms(uint32_t ms) {
  delay(ms);
}

} // extern "C"

