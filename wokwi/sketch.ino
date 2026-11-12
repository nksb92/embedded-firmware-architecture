#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "bad_helpers.h"

// ============================================================================
// HARDWARE PIN DEFINITIONS (RP2040 Smart LED Controller)
// ============================================================================
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64

#define PIN_SDA          4   // I2C0 SDA (Logic Analyzer CH0)
#define PIN_SCL          5   // I2C0 SCL
#define PIN_ENC_A        10  // Quadrature CLK / A (Logic Analyzer CH1)
#define PIN_ENC_B        11  // Quadrature DT / B
#define PIN_ENC_SW       12  // Rotary Encoder Pushbutton (Active LOW)
#define PIN_FAULT_BTN    13  // Over-temp / E-stop Fault Button (Active LOW)
#define PIN_WS2812       15  // WS2812B 8-Pixel LED Strip Data
#define PIN_DBG_DISPLAY  18  // Logic Analyzer CH2: High during OLED update (~95ms)
#define PIN_DBG_LED      19  // Logic Analyzer CH3: High during LED calculation & show
#define PIN_DBG_INPUT    20  // Logic Analyzer CH4: Pulsed high during input polling

#define NUM_LEDS         8

// ============================================================================
// PERIPHERALS
// ============================================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel strip(NUM_LEDS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

// ============================================================================
// SPAGHETTI FAILURE MODE 1: UNGUARDED EXTERN GLOBAL VARIABLE
// Declared in bad_helpers.h as: extern uint8_t g_brightness;
// Subtraction 0 - 10 wraps uint8_t straight to 246!
// ============================================================================
uint8_t g_brightness = 50; // Initial brightness: 50%

// ============================================================================
// SPAGHETTI FAILURE MODE 3: CONFLICTING BOOLEAN FLAGS (2^5 = 32 States)
// ============================================================================
bool isOutputOn = true;
bool isMenuOpen = false;
bool isEditing  = false;
bool hasFault   = false;
bool isLocked   = false;

// Polling tracking state
int lastEncA = HIGH;
bool lastSwState = HIGH;
bool lastFaultBtnState = HIGH;
uint16_t animStep = 0;

// Helper: Color wheel for 8-pixel WS2812B chase animation
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void setup() {
  Serial.begin(115200);

  // Inputs with internal pull-ups
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_FAULT_BTN, INPUT_PULLUP);

  // Logic analyzer debug pins
  pinMode(PIN_DBG_DISPLAY, OUTPUT);
  pinMode(PIN_DBG_LED, OUTPUT);
  pinMode(PIN_DBG_INPUT, OUTPUT);

  digitalWrite(PIN_DBG_DISPLAY, LOW);
  digitalWrite(PIN_DBG_LED, LOW);
  digitalWrite(PIN_DBG_INPUT, LOW);

  // I2C OLED display initialization
  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 8);
    display.println("Smart LED Controller");
    display.println("Firmware v0.1");
    display.println("Prototype booting...");
    display.display();
  }

  // Addressable LED strip initialization
  strip.begin();
  strip.setBrightness(g_brightness);
  strip.show();

  lastEncA = digitalRead(PIN_ENC_A);
  lastSwState = digitalRead(PIN_ENC_SW);
  lastFaultBtnState = digitalRead(PIN_FAULT_BTN);

  Serial.println("v0.1_spaghetti_controller ready.");
}

void loop() {
  // --------------------------------------------------------------------------
  // STEP 1: INPUT POLLING (PULSE CH4)
  // In this slow superloop, polling here is blind for ~95ms every loop iteration!
  // --------------------------------------------------------------------------
  digitalWrite(PIN_DBG_INPUT, HIGH);

  // Read encoder pushbutton (toggles into / out of EDIT mode)
  bool swPressed = (digitalRead(PIN_ENC_SW) == LOW);
  if (swPressed && (lastSwState == HIGH)) {
    if (!isEditing) {
      isMenuOpen = true;
      isEditing = true;
    } else {
      isEditing = false;
      isMenuOpen = false;
    }
  }
  lastSwState = swPressed ? LOW : HIGH;

  // Read hardware fault button (latching toggle on press)
  bool faultBtnPressed = (digitalRead(PIN_FAULT_BTN) == LOW);
  if (faultBtnPressed && (lastFaultBtnState == HIGH)) {
    hasFault = !hasFault;
    if (hasFault) {
      isOutputOn = false; // E-stop cuts output in normal mode
    } else {
      isOutputOn = true;
    }
  }
  lastFaultBtnState = faultBtnPressed ? LOW : HIGH;

  // Read rotary encoder quadrature signals
  int currentEncA = digitalRead(PIN_ENC_A);
  if (currentEncA != lastEncA && currentEncA == LOW) {
    if (digitalRead(PIN_ENC_B) != currentEncA) {
      // Clockwise rotation (increase brightness)
      if (isEditing) {
        g_brightness += 10;
        // SPAGHETTI FAILURE MODE 3 (GHOST STATE BUG):
        // Developer wanted live feedback while editing and forgot to check hasFault!
        // Turning the knob re-enables output even while hasFault is true!
        isOutputOn = true;
      }
    } else {
      // Counter-Clockwise rotation (decrease brightness)
      if (isEditing) {
        // SPAGHETTI FAILURE MODE 1 (INTEGER UNDERFLOW BUG):
        // Unguarded subtraction on uint8_t: 0 - 10 wraps straight to 246!
        g_brightness -= 10;
        // GHOST STATE BUG: Forgets to check hasFault!
        isOutputOn = true;
      }
    }
  }
  lastEncA = currentEncA;

  digitalWrite(PIN_DBG_INPUT, LOW);

  // --------------------------------------------------------------------------
  // STEP 2: REAL-TIME LED ANIMATION (PULSE CH3)
  // --------------------------------------------------------------------------
  digitalWrite(PIN_DBG_LED, HIGH);

  if (hasFault && !isOutputOn) {
    // Normal fault state: LEDs completely dark
    strip.clear();
    strip.show();
  } else if (isOutputOn) {
    // If hasFault is true BUT isOutputOn was re-enabled by the ghost state bug,
    // this branch runs and illegally drives the LEDs!
    strip.setBrightness(g_brightness);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / NUM_LEDS) + animStep) & 255));
    }
    strip.show();
    animStep += 4;
  } else {
    strip.clear();
    strip.show();
  }

  digitalWrite(PIN_DBG_LED, LOW);

  // --------------------------------------------------------------------------
  // STEP 3: BLOCKING DISPLAY UPDATE & HARDWARE LOCK-IN (HOLD CH2 HIGH ~95ms)
  // --------------------------------------------------------------------------
  digitalWrite(PIN_DBG_DISPLAY, HIGH);

  // SPAGHETTI FAILURE MODE 2: HARDCODED HARDWARE LOCK-IN
  // Direct I2C commands and SSD1306 register bytes inside UI branches!
  if (hasFault) {
    Wire.beginTransmission(0x3C);
    Wire.write(0x00); // Command stream
    Wire.write(0x81); // Set Contrast Control
    Wire.write(0xFF); // Max contrast for fault alert
    Wire.endTransmission();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(12, 0);
    display.println("! FAULT !");
    display.setTextSize(1);
    display.drawLine(0, 18, 127, 18, SSD1306_WHITE);
    display.setCursor(0, 24);
    display.println("OVER-TEMP / HARDWARE");
    display.setCursor(0, 36);
    if (isEditing) {
      display.print("EDIT MODE: ");
      display.print(g_brightness);
      display.println("%");
    } else {
      display.println("OUTPUTS DISABLED");
    }
    display.setCursor(0, 50);
    display.println("Press FAULT to clear");
  } else if (isEditing) {
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);
    Wire.write(0x81); // Contrast command
    Wire.write(g_brightness); // Tracking brightness
    Wire.endTransmission();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("=== EDIT SETTINGS ===");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 18);
    display.println("LED BRIGHTNESS:");
    display.setTextSize(2);
    display.setCursor(20, 32);
    display.print(g_brightness);
    display.println("%");
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println("Turn: Adj | SW: Exit");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(4, 0);
    display.println("SMART LED CONTROLLER");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 16);
    display.print("Output:     ");
    display.println(isOutputOn ? "ACTIVE" : "DISABLED");
    display.setCursor(0, 28);
    display.print("Brightness: ");
    display.print(g_brightness);
    display.println("%");
    display.setCursor(0, 40);
    display.print("State:      ");
    display.println(isLocked ? "LOCKED" : "READY");
    display.setCursor(0, 52);
    display.println("Click SW for Settings");
  }

  // Pushes 1024-byte framebuffer over 100 kHz I2C (~95 ms blocking transfer on RP2040)
  display.display();

  digitalWrite(PIN_DBG_DISPLAY, LOW);

  // --------------------------------------------------------------------------
  // STEP 4: BLOCKING DELAY (STARVES POLLING LOOP FURTHER)
  // --------------------------------------------------------------------------
  delay(20);
}
