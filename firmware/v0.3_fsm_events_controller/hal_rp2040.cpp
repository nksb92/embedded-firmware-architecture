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

static Adafruit_NeoPixel strip(MAX_WS2812_LEDS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

static int Rp2040PinGet(HalGpioPin pin) {
  switch (pin) {
    case kHalPinEncA:       return PIN_ENC_A;
    case kHalPinEncB:       return PIN_ENC_B;
    case kHalPinEncSw:      return PIN_ENC_SW;
    case kHalPinFaultBtn:   return PIN_FAULT_BTN;
    case kHalPinDbgDisplay: return PIN_DBG_DISPLAY;
    case kHalPinDbgLed:     return PIN_DBG_LED;
    case kHalPinDbgInput:   return PIN_DBG_INPUT;
    default:                return -1;
  }
}

extern "C" {

void HalInit(void) {
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
  Wire.setTimeout(5); // 5 ms bus timeout (prevents 1000 ms default hang per packet if I2C bus is stuck)
  Wire.begin();
  Wire.setClock(100000);

  strip.begin();
  strip.show();
}

HalGpioLevel HalGpioGet(HalGpioPin pin) {
  int p = Rp2040PinGet(pin);
  if (p < 0) return kHalGpioLow;
  return (digitalRead(p) == HIGH) ? kHalGpioHigh : kHalGpioLow;
}

void HalGpioSet(HalGpioPin pin, HalGpioLevel level) {
  int p = Rp2040PinGet(pin);
  if (p >= 0) {
    digitalWrite(p, (level == kHalGpioHigh) ? HIGH : LOW);
  }
}

bool HalI2cWrite(uint8_t addr_7bit, const uint8_t *data, size_t len) {
  Wire.beginTransmission(addr_7bit);
  size_t written = Wire.write(data, len);
  return (Wire.endTransmission() == 0) && (written == len);
}

void HalWs2812Write(const uint8_t *grb_data, size_t num_pixels) {
  if (num_pixels > MAX_WS2812_LEDS) num_pixels = MAX_WS2812_LEDS;
  for (size_t i = 0; i < num_pixels; i++) {
    uint8_t g = grb_data[i * 3 + 0];
    uint8_t r = grb_data[i * 3 + 1];
    uint8_t b = grb_data[i * 3 + 2];
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

uint32_t HalTimeMsGet(void) {
  return millis();
}

void HalDelayMs(uint32_t ms) {
  delay(ms);
}

} // extern "C"

