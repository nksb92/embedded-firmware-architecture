#include "drv_ssd1306.h"
#include "hal_interface.h"
#include <Adafruit_GFX.h>
#include <string.h>

// ============================================================================
// LAYER 2 (DRIVER): SSD1306 OLED DRIVER IMPLEMENTATION
// Encapsulates the SSD1306 command sequences and 1024-byte 1-bpp framebuffer.
// Never touches hardware registers or Wire directly — all bus traffic routes
// exclusively through the Layer 3 HAL contract (hal_i2c_write).
// ============================================================================

#define SSD1306_I2C_ADDR  0x3C
#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT    64

static GFXcanvas1 s_canvas(SSD1306_WIDTH, SSD1306_HEIGHT);

static bool ssd1306_send_cmd(uint8_t cmd) {
  uint8_t buf[2] = { 0x00, cmd }; // 0x00 = Co=0, D/C#=0 (Command stream)
  return hal_i2c_write(SSD1306_I2C_ADDR, buf, sizeof(buf));
}

static bool ssd1306_set_contrast(uint8_t contrast) {
  uint8_t buf[3] = { 0x00, 0x81, contrast }; // 0x81 = Set Contrast Control
  return hal_i2c_write(SSD1306_I2C_ADDR, buf, sizeof(buf));
}

static void ssd1306_flush_framebuffer(void) {
  // Set column address range (0..127) and page address range (0..7)
  uint8_t addr_cmds[] = {
    0x00,
    0x21, 0, SSD1306_WIDTH - 1,
    0x22, 0, (SSD1306_HEIGHT / 8) - 1
  };
  // Early abort: if the display is disconnected/miswired and NACKs or times out,
  // abort immediately instead of stalling across all 64 page chunks!
  if (!hal_i2c_write(SSD1306_I2C_ADDR, addr_cmds, sizeof(addr_cmds))) {
    return;
  }

  // Convert GFXcanvas1 horizontal bitmask into SSD1306 vertical pages
  // and stream 1024 bytes in 16-byte I2C chunks over the 100 kHz HAL bus (~95 ms)
  const uint8_t *gfx_buf = s_canvas.getBuffer();
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
      if (!hal_i2c_write(SSD1306_I2C_ADDR, packet, sizeof(packet))) {
        return; // Abort remaining chunks on bus error/timeout
      }
    }
  }
}

extern "C" {

void drv_ssd1306_init(void) {
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
    if (!ssd1306_send_cmd(init_seq[i])) {
      break; // Do not stall for 25 seconds on boot if OLED is miswired
    }
  }
  s_canvas.fillScreen(0);
  ssd1306_flush_framebuffer();
}

void drv_ssd1306_render(uint8_t screen_mode, bool is_output_on, int16_t brightness, bool is_locked) {
  hal_gpio_write(HAL_PIN_DBG_DISPLAY, HAL_GPIO_HIGH);

  s_canvas.fillScreen(0);
  s_canvas.setTextColor(1);

  if (screen_mode == 2) {
    // Hardware Fault Alert Screen
    ssd1306_set_contrast(0xFF);
    s_canvas.setTextSize(2);
    s_canvas.setCursor(12, 0);
    s_canvas.println("! FAULT !");
    s_canvas.setTextSize(1);
    s_canvas.drawLine(0, 18, 127, 18, 1);
    s_canvas.setCursor(0, 24);
    s_canvas.println("OVER-TEMP / HARDWARE");
    s_canvas.setCursor(0, 36);
    s_canvas.print("BRIGHTNESS: ");
    s_canvas.print(brightness);
    s_canvas.println("%");
    s_canvas.setCursor(0, 50);
    s_canvas.println("Press FAULT to clear");
  } else if (screen_mode == 1) {
    // Edit Settings Screen
    ssd1306_set_contrast((uint8_t)(brightness * 255 / 100));
    s_canvas.setTextSize(1);
    s_canvas.setCursor(0, 0);
    s_canvas.println("=== EDIT SETTINGS ===");
    s_canvas.drawLine(0, 10, 127, 10, 1);
    s_canvas.setCursor(0, 18);
    s_canvas.println("LED BRIGHTNESS:");
    s_canvas.setTextSize(2);
    s_canvas.setCursor(20, 32);
    s_canvas.print(brightness);
    s_canvas.println("%");
    s_canvas.setTextSize(1);
    s_canvas.setCursor(0, 52);
    s_canvas.println("Turn: Adj | SW: Exit");
  } else {
    // Normal Status Screen
    ssd1306_set_contrast(0xCF);
    s_canvas.setTextSize(1);
    s_canvas.setCursor(4, 0);
    s_canvas.println("SMART LED CONTROLLER");
    s_canvas.drawLine(0, 10, 127, 10, 1);
    s_canvas.setCursor(0, 16);
    s_canvas.print("Output:     ");
    s_canvas.println(is_output_on ? "ACTIVE" : "DISABLED");
    s_canvas.setCursor(0, 28);
    s_canvas.print("Brightness: ");
    s_canvas.print(brightness);
    s_canvas.println("%");
    s_canvas.setCursor(0, 40);
    s_canvas.print("State:      ");
    s_canvas.println(is_locked ? "LOCKED" : "READY");
    s_canvas.setCursor(0, 52);
    s_canvas.println("Click SW for Settings");
  }

  // Pushes 1024-byte framebuffer over 100 kHz I2C via HAL (~95 ms blocking transfer)
  ssd1306_flush_framebuffer();

  hal_gpio_write(HAL_PIN_DBG_DISPLAY, HAL_GPIO_LOW);
}

} // extern "C"

