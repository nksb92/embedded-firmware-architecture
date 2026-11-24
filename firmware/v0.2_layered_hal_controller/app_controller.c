#include "app_controller.h"
#include "drv_led.h"
#include "drv_ssd1306.h"
#include "hal_interface.h"

// ============================================================================
// LAYER 1: APPLICATION STATE ENCAPSULATION
// Data is protected from other files using static file scope.
// NOTE (Act 5 Reality Check): Hiding 5 boolean flags inside a static struct
// still creates 2^5 = 32 states and preserves the runtime ghost state bug!
// This cliffhanger sets up Video #2 (Finite State Machines & Event Queues).
// ============================================================================
typedef struct {
  bool is_output_on;
  bool is_menu_open;
  bool is_editing;
  bool has_fault;
  bool is_locked;

  HalGpioLevel last_enc_a;
  HalGpioLevel last_sw;
  HalGpioLevel last_fault_btn;
} AppContext;

static AppContext app_ctx;

void AppInit(void) {
  HalInit();
  DrvLedInit();
  DrvSsd1306Init();

  app_ctx.is_output_on = true;
  app_ctx.is_menu_open = false;
  app_ctx.is_editing   = false;
  app_ctx.has_fault    = false;
  app_ctx.is_locked    = false;

  app_ctx.last_enc_a     = HalGpioGet(kHalPinEncA);
  app_ctx.last_sw        = HalGpioGet(kHalPinEncSw);
  app_ctx.last_fault_btn = HalGpioGet(kHalPinFaultBtn);
}

void AppUpdate(void) {
  // --------------------------------------------------------------------------
  // STEP 1: INPUT POLLING (PULSE CH4)
  // Still polling in the superloop! Misses pulses during OLED redraws.
  // --------------------------------------------------------------------------
  HalGpioSet(kHalPinDbgInput, kHalGpioHigh);

  // Read encoder pushbutton
  HalGpioLevel sw_level = HalGpioGet(kHalPinEncSw);
  if (sw_level == kHalGpioLow && app_ctx.last_sw == kHalGpioHigh) {
    if (!app_ctx.is_editing) {
      app_ctx.is_menu_open = true;
      app_ctx.is_editing   = true;
    } else {
      app_ctx.is_editing   = false;
      app_ctx.is_menu_open = false;
    }
  }
  app_ctx.last_sw = sw_level;

  // Read hardware fault button
  HalGpioLevel fault_btn_level = HalGpioGet(kHalPinFaultBtn);
  if (fault_btn_level == kHalGpioLow && app_ctx.last_fault_btn == kHalGpioHigh) {
    app_ctx.has_fault = !app_ctx.has_fault;
    if (app_ctx.has_fault) {
      app_ctx.is_output_on = false; // E-stop cuts output in normal mode
    } else {
      app_ctx.is_output_on = true;
    }
  }
  app_ctx.last_fault_btn = fault_btn_level;

  // Read rotary encoder
  HalGpioLevel enc_a = HalGpioGet(kHalPinEncA);
  if (enc_a != app_ctx.last_enc_a && enc_a == kHalGpioLow) {
    HalGpioLevel enc_b = HalGpioGet(kHalPinEncB);
    int16_t current_brt = DrvLedBrightnessGet();

    if (enc_b != enc_a) {
      // Clockwise
      if (app_ctx.is_editing) {
        // Validated API: DrvLedBrightnessSet cleanly clamps to 100 max
        DrvLedBrightnessSet(current_brt + 10);
        // CONTRADICTORY BOOLEAN BRANCH (ACT 5 CLIFFHANGER):
        // Re-enables output without checking has_fault!
        app_ctx.is_output_on = true;
      }
    } else {
      // Counter-Clockwise
      if (app_ctx.is_editing) {
        // ACT 4 PAYOFF:
        // Even when passing current_brt - 10 (e.g. 0 - 10 = -10),
        // DrvLedBrightnessSet clamps cleanly to 0%! Underflow bug is dead!
        DrvLedBrightnessSet(current_brt - 10);
        // CONTRADICTORY BOOLEAN BRANCH (ACT 5 CLIFFHANGER):
        // Re-enables output without checking has_fault!
        app_ctx.is_output_on = true;
      }
    }
  }
  app_ctx.last_enc_a = enc_a;

  HalGpioSet(kHalPinDbgInput, kHalGpioLow);

  // --------------------------------------------------------------------------
  // STEP 2: LED DRIVER UPDATE
  // --------------------------------------------------------------------------
  bool render_output = app_ctx.is_output_on;
  if (app_ctx.has_fault && !app_ctx.is_output_on) {
    render_output = false;
  }
  DrvLedFrameRender(render_output);

  // --------------------------------------------------------------------------
  // STEP 3: DISPLAY DRIVER UPDATE (~95ms blocking transfer)
  // --------------------------------------------------------------------------
  uint8_t screen_mode = 0;
  if (app_ctx.has_fault) {
    screen_mode = 2; // Fault alert screen
  } else if (app_ctx.is_editing) {
    screen_mode = 1; // Edit brightness screen
  }
  DrvSsd1306Render(screen_mode, app_ctx.is_output_on, DrvLedBrightnessGet(), app_ctx.is_locked);

  // --------------------------------------------------------------------------
  // STEP 4: SUPERLOOP DELAY
  // --------------------------------------------------------------------------
  HalDelayMs(20);
}
