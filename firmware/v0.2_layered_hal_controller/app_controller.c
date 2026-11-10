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

  hal_gpio_level_t last_enc_a;
  hal_gpio_level_t last_sw;
  hal_gpio_level_t last_fault_btn;
} app_context_t;

static app_context_t s_app;

void app_init(void) {
  hal_init();
  drv_led_init();
  drv_ssd1306_init();

  s_app.is_output_on = true;
  s_app.is_menu_open = false;
  s_app.is_editing  = false;
  s_app.has_fault   = false;
  s_app.is_locked   = false;

  s_app.last_enc_a     = hal_gpio_read(HAL_PIN_ENC_A);
  s_app.last_sw        = hal_gpio_read(HAL_PIN_ENC_SW);
  s_app.last_fault_btn = hal_gpio_read(HAL_PIN_FAULT_BTN);
}

void app_update(void) {
  // --------------------------------------------------------------------------
  // STEP 1: INPUT POLLING (PULSE CH4)
  // Still polling in the superloop! Misses pulses during OLED redraws.
  // --------------------------------------------------------------------------
  hal_gpio_write(HAL_PIN_DBG_INPUT, HAL_GPIO_HIGH);

  // Read encoder pushbutton
  hal_gpio_level_t sw_level = hal_gpio_read(HAL_PIN_ENC_SW);
  if (sw_level == HAL_GPIO_LOW && s_app.last_sw == HAL_GPIO_HIGH) {
    if (!s_app.is_editing) {
      s_app.is_menu_open = true;
      s_app.is_editing   = true;
    } else {
      s_app.is_editing   = false;
      s_app.is_menu_open = false;
    }
  }
  s_app.last_sw = sw_level;

  // Read hardware fault button
  hal_gpio_level_t fault_btn_level = hal_gpio_read(HAL_PIN_FAULT_BTN);
  if (fault_btn_level == HAL_GPIO_LOW && s_app.last_fault_btn == HAL_GPIO_HIGH) {
    s_app.has_fault = !s_app.has_fault;
    if (s_app.has_fault) {
      s_app.is_output_on = false; // E-stop cuts output in normal mode
    } else {
      s_app.is_output_on = true;
    }
  }
  s_app.last_fault_btn = fault_btn_level;

  // Read rotary encoder
  hal_gpio_level_t enc_a = hal_gpio_read(HAL_PIN_ENC_A);
  if (enc_a != s_app.last_enc_a && enc_a == HAL_GPIO_LOW) {
    hal_gpio_level_t enc_b = hal_gpio_read(HAL_PIN_ENC_B);
    int16_t current_brt = drv_led_get_brightness();

    if (enc_b != enc_a) {
      // Clockwise
      if (s_app.is_editing) {
        // Validated API: drv_led_set_brightness cleanly clamps to 100 max
        drv_led_set_brightness(current_brt + 10);
        // CONTRADICTORY BOOLEAN BRANCH (ACT 5 CLIFFHANGER):
        // Re-enables output without checking has_fault!
        s_app.is_output_on = true;
      }
    } else {
      // Counter-Clockwise
      if (s_app.is_editing) {
        // ACT 4 PAYOFF:
        // Even when passing current_brt - 10 (e.g. 0 - 10 = -10),
        // drv_led_set_brightness clamps cleanly to 0%! Underflow bug is dead!
        drv_led_set_brightness(current_brt - 10);
        // CONTRADICTORY BOOLEAN BRANCH (ACT 5 CLIFFHANGER):
        s_app.is_output_on = true;
      }
    }
  }
  s_app.last_enc_a = enc_a;

  hal_gpio_write(HAL_PIN_DBG_INPUT, HAL_GPIO_LOW);

  // --------------------------------------------------------------------------
  // STEP 2: LED DRIVER UPDATE
  // --------------------------------------------------------------------------
  bool render_output = s_app.is_output_on;
  if (s_app.has_fault && !s_app.is_output_on) {
    render_output = false;
  }
  drv_led_render_frame(render_output);

  // --------------------------------------------------------------------------
  // STEP 3: DISPLAY DRIVER UPDATE (~95ms blocking transfer)
  // --------------------------------------------------------------------------
  uint8_t screen_mode = 0;
  if (s_app.has_fault) {
    screen_mode = 2; // Fault alert screen
  } else if (s_app.is_editing) {
    screen_mode = 1; // Edit brightness screen
  }
  drv_ssd1306_render(screen_mode, s_app.is_output_on, drv_led_get_brightness(), s_app.is_locked);

  // --------------------------------------------------------------------------
  // STEP 4: SUPERLOOP DELAY
  // --------------------------------------------------------------------------
  hal_delay_ms(20);
}

