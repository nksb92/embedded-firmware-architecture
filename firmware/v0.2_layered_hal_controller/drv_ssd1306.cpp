#include "drv_ssd1306.h"
#include "hal_interface.h"
#include <Adafruit_GFX.h>
#include <string.h>

// ============================================================================
// LAYER 2 (DRIVER): SSD1306 OLED DRIVER IMPLEMENTATION
// Encapsulates the SSD1306 command sequences and 1024-byte 1-bpp framebuffer.
// Never touches hardware registers or Wire directly — all bus traffic routes
// exclusively through the Layer 3 HAL contract (HalI2cWrite).
// ============================================================================

#define SSD1306_I2C_ADDR  0x3C
#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT    64

static GFXcanvas1 canvas(SSD1306_WIDTH, SSD1306_HEIGHT);

static bool Ssd1306CmdSend(uint8_t cmd) {
  uint8_t buf[2] = { 0x00, cmd }; // 0x00 = Co=0, D/C#=0 (Command stream)
  return HalI2cWrite(SSD1306_I2C_ADDR, buf, sizeof(buf));
}

static bool Ssd1306ContrastSet(uint8_t contrast) {
  uint8_t buf[3] = { 0x00, 0x81, contrast }; // 0x81 = Set Contrast Control
  return HalI2cWrite(SSD1306_I2C_ADDR, buf, sizeof(buf));
}

static void Ssd1306FramebufferFlush(void) {
  // Set column address range (0..127) and page address range (0..7)
  uint8_t addr_cmds[] = {
    0x00,
    0x21, 0, SSD1306_WIDTH - 1,
    0x22, 0, (SSD1306_HEIGHT / 8) - 1
  };
  // Early abort: if the display is disconnected/miswired and NACKs or times out,
  // abort immediately instead of stalling across all 64 page chunks!
  if (!HalI2cWrite(SSD1306_I2C_ADDR, addr_cmds, sizeof(addr_cmds))) {
    return;
  }

  // Convert GFXcanvas1 horizontal bitmask into SSD1306 vertical pages
  // and stream 1024 bytes in 16-byte I2C chunks over the 100 kHz HAL bus (~95 ms)
  const uint8_t *gfx_buf = canvas.getBuffer();
  for (uint8_t page = 0; page < 8; page++) {
    for (uint8_t col_base = 0; col_base < SSD1306_WIDTH; col_base += 16) {
      uint8_t packet[17];
      packet[0] = 0x40; // 0x40 = Co=0, D/C#=1 (GDDRAM Data stream)
      for (uint8_t c = 0; c < 16; c++) {
        uint8_t x = col_base + c;
        uint8_t page_byte = 0;
        for (uint8_t bit = 0; bit < 8; bit++) {
          uint8_t y = page * 8 + bit;
          if (gfx_buf[(y * (SSD1306_WIDTH / 8)) + (x / 8)] & (0x80 >> (x & 7))) {
            page_byte |= (1 << bit);
          }
        }
        packet[1 + c] = page_byte;
      }
      if (!HalI2cWrite(SSD1306_I2C_ADDR, packet, sizeof(packet))) {
        return; // Abort remaining chunks on bus error/timeout
      }
    }
  }
}

extern "C" {

void DrvSsd1306Init(void) {
  static const uint8_t init_seq[] = {
    0xAE,       // Display OFF
    0xD5, 0x80, // Set Display Clock Divide Ratio
    0xA8, 0x3F, // Set Multiplex Ratio (64MUX)
    0xD3, 0x00, // Set Display Offset
    0x40,       // Set Start Line (0)
    0x8D, 0x14, // Enable Charge Pump
    0x20, 0x00, // Horizontal Addressing Mode
    0xA1,       // Segment Re-map (col 127 mapped to SEG0)
    0xC8,       // COM Output Scan Direction Remapped
    0xDA, 0x12, // COM Pins Hardware Configuration
    0x81, 0xCF, // Set Contrast Control
    0xD9, 0xF1, // Set Pre-charge Period
    0xDB, 0x40, // Set VCOMH Deselect Level
    0xA4,       // Entire Display ON (resume from RAM)
    0xA6,       // Normal Display (non-inverted)
    0xAF        // Display ON
  };
  for (size_t i = 0; i < sizeof(init_seq); i++) {
    if (!Ssd1306CmdSend(init_seq[i])) {
      break; // Do not stall for 25 seconds on boot if OLED is miswired
    }
  }
  canvas.fillScreen(0);
  Ssd1306FramebufferFlush();
}

void DrvSsd1306Render(uint8_t screen_mode, bool is_output_on, int16_t brightness, bool is_locked) {
  HalGpioSet(kHalPinDbgDisplay, kHalGpioHigh);

  canvas.fillScreen(0);
  canvas.setTextColor(1);

  if (screen_mode == 2) {
    // Hardware Fault Alert Screen
    Ssd1306ContrastSet(0xFF);
    canvas.setTextSize(2);
    canvas.setCursor(12, 0);
    canvas.println("! FAULT !");
    canvas.setTextSize(1);
    canvas.drawLine(0, 18, 127, 18, 1);
    canvas.setCursor(0, 24);
    canvas.println("OVER-TEMP / HARDWARE");
    canvas.setCursor(0, 36);
    canvas.print("BRIGHTNESS: ");
    canvas.print(brightness);
    canvas.println("%");
    canvas.setCursor(0, 50);
    canvas.println("Press FAULT to clear");
  } else if (screen_mode == 1) {
    // Edit Settings Screen
    Ssd1306ContrastSet((uint8_t)(brightness * 255 / 100));
    canvas.setTextSize(1);
    canvas.setCursor(0, 0);
    canvas.println("=== EDIT SETTINGS ===");
    canvas.drawLine(0, 10, 127, 10, 1);
    canvas.setCursor(0, 18);
    canvas.println("LED BRIGHTNESS:");
    canvas.setTextSize(2);
    canvas.setCursor(20, 32);
    canvas.print(brightness);
    canvas.println("%");
    canvas.setTextSize(1);
    canvas.setCursor(0, 52);
    canvas.println("Turn: Adj | SW: Exit");
  } else {
    // Normal Status Screen
    Ssd1306ContrastSet(0xCF);
    canvas.setTextSize(1);
    canvas.setCursor(4, 0);
    canvas.println("SMART LED CONTROLLER");
    canvas.drawLine(0, 10, 127, 10, 1);
    canvas.setCursor(0, 16);
    canvas.print("Output:     ");
    canvas.println(is_output_on ? "ACTIVE" : "DISABLED");
    canvas.setCursor(0, 28);
    canvas.print("Brightness: ");
    canvas.print(brightness);
    canvas.println("%");
    canvas.setCursor(0, 40);
    canvas.print("State:      ");
    canvas.println(is_locked ? "LOCKED" : "READY");
    canvas.setCursor(0, 52);
    canvas.println("Click SW for Settings");
  }

  // Pushes 1024-byte framebuffer over 100 kHz I2C via HAL (~95 ms blocking transfer)
  Ssd1306FramebufferFlush();

  HalGpioSet(kHalPinDbgDisplay, kHalGpioLow);
}

} // extern "C"

