#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LAYER 1: APPLICATION CONTROLLER
// High-level system logic. Encapsulates state in private static variables.
// Uses Layer 2 drivers (drv_led, drv_ssd1306) and Layer 3 HAL (hal_interface).
// Zero raw hardware register names or peripheral addresses.
// ============================================================================

void app_init(void);
void app_update(void);

#ifdef __cplusplus
}
#endif

#endif // APP_CONTROLLER_H

