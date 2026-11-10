#ifndef BAD_HELPERS_H
#define BAD_HELPERS_H

#include <stdint.h>

// ============================================================================
// SPAGHETTI FAILURE MODE 1: UNGUARDED EXTERN GLOBAL VARIABLE
// Any file can include this header and directly mutate g_brightness without bounds checks.
// Subtraction 0 - 10 wraps uint8_t straight to 246!
// ============================================================================
extern uint8_t g_brightness;

#endif // BAD_HELPERS_H

