#include "app_controller.h"
#include "drv_led.h"
#include "drv_ssd1306.h"
#include "hal_interface.h"

// ============================================================================
// LAYER 1: APPLICATION CONTROLLER — FINITE STATE MACHINE & FIFO EVENT QUEUE
// Refactored from v0.2 (5 tangled boolean flags -> 2^5 = 32 implicit states)
// into 4 mutually exclusive enum states and a bounded static FIFO ring buffer.
// Zero lines modified in drv_led, drv_ssd1306, hal_interface, or hal_rp2040.
// ============================================================================

#define APP_EVENT_QUEUE_CAPACITY 16
#define APP_LOCK_HOLD_MS         1000u

typedef enum {
  kAppStateRunning = 0,
  kAppStateEditing,
  kAppStateLocked,
  kAppStateFault
} AppState;

typedef enum {
  kEvtEncCw = 0,
  kEvtEncCcw,
  kEvtEncShortPress,
  kEvtLockToggle,
  kEvtFaultToggle
} AppEvent;

// ----------------------------------------------------------------------------
// FIXED-SIZE STATIC FIFO EVENT QUEUE (RING BUFFER)
// Deterministic memory, zero heap allocation, bounded overflow rejection.
// NOTE (Act 4 Cliffhanger): head, tail, and shared count are safe here only
// because producer and consumer run sequentially in the superloop. Moving
// EventQueuePush() into a hardware ISR in Part 3 will expose a read-modify-write
// race condition on count, motivating a lock-free SPSC ring buffer.
// ----------------------------------------------------------------------------
typedef struct {
  AppEvent buffer[APP_EVENT_QUEUE_CAPACITY];
  uint8_t head;
  uint8_t tail;
  uint8_t count;
} AppEventQueue;

static AppEventQueue event_queue;

static void EventQueueInit(void) {
  event_queue.head  = 0;
  event_queue.tail  = 0;
  event_queue.count = 0;
}

static bool EventQueuePush(AppEvent evt) {
  if (event_queue.count >= APP_EVENT_QUEUE_CAPACITY) {
    return false;
  }
  event_queue.buffer[event_queue.tail] = evt;
  event_queue.tail = (uint8_t)((event_queue.tail + 1u) % APP_EVENT_QUEUE_CAPACITY);
  event_queue.count++;
  return true;
}

static bool EventQueuePop(AppEvent *evt) {
  if (event_queue.count == 0) {
    return false;
  }
  *evt = event_queue.buffer[event_queue.head];
  event_queue.head = (uint8_t)((event_queue.head + 1u) % APP_EVENT_QUEUE_CAPACITY);
  event_queue.count--;
  return true;
}

// ----------------------------------------------------------------------------
// PRIVATE APPLICATION CONTEXT
// All 5 boolean flags from v0.2 are replaced by a single AppState enum.
// ----------------------------------------------------------------------------
typedef struct {
  AppState current_state;
  uint8_t screen_mode;

  HalGpioLevel last_enc_a;
  HalGpioLevel last_sw;
  HalGpioLevel last_fault_btn;
  uint32_t sw_press_start_ms;
  bool sw_long_press_handled;
} AppContext;

static AppContext app_ctx;

// ----------------------------------------------------------------------------
// FSM ENTRY / EXIT ACTIONS & CENTRALIZED TRANSITION HELPER
// ----------------------------------------------------------------------------
static void StateOnExit(AppState state) {
  switch (state) {
    case kAppStateRunning:
    case kAppStateEditing:
    case kAppStateLocked:
    case kAppStateFault:
      break;
  }
}

static void StateOnEnter(AppState state) {
  switch (state) {
    case kAppStateRunning:
      app_ctx.screen_mode = 0; // Normal status screen
      break;

    case kAppStateEditing:
      app_ctx.screen_mode = 1; // Edit brightness screen
      break;

    case kAppStateLocked:
      app_ctx.screen_mode = 0; // Normal status screen with LOCKED indicator
      break;

    case kAppStateFault:
      app_ctx.screen_mode = 2; // Fault alert screen
      // Immediately kill LED output on fault entry regardless of prior state
      DrvLedFrameRender(false);
      break;
  }
}

static void StateTransitionTo(AppState next_state) {
  if (app_ctx.current_state == next_state) {
    return;
  }
  StateOnExit(app_ctx.current_state);
  app_ctx.current_state = next_state;
  StateOnEnter(next_state);
}

// ----------------------------------------------------------------------------
// FSM DISPATCHER (CONSUMER)
// Hybrid architecture: readable switch/case state handlers with an HSM-style
// parent OPERATIONAL superstate guard catching FAULT_TOGGLE in one place.
// ----------------------------------------------------------------------------
static void FsmDispatch(AppEvent evt) {
  // Parent OPERATIONAL superstate guard: any non-fault state jumps to FAULT
  if (app_ctx.current_state != kAppStateFault && evt == kEvtFaultToggle) {
    StateTransitionTo(kAppStateFault);
    return;
  }

  switch (app_ctx.current_state) {
    case kAppStateRunning:
      if (evt == kEvtEncShortPress) {
        StateTransitionTo(kAppStateEditing);
      } else if (evt == kEvtLockToggle) {
        StateTransitionTo(kAppStateLocked);
      }
      break;

    case kAppStateEditing:
      if (evt == kEvtEncShortPress) {
        StateTransitionTo(kAppStateRunning);
      } else if (evt == kEvtEncCw) {
        DrvLedBrightnessSet(DrvLedBrightnessGet() + 10);
      } else if (evt == kEvtEncCcw) {
        DrvLedBrightnessSet(DrvLedBrightnessGet() - 10);
      }
      break;

    case kAppStateLocked:
      if (evt == kEvtLockToggle) {
        StateTransitionTo(kAppStateRunning);
      }
      break;

    case kAppStateFault:
      // Only a fault-clear press leaves FAULT; encoder events are ignored
      if (evt == kEvtFaultToggle) {
        StateTransitionTo(kAppStateRunning);
      }
      break;
  }
}

// ----------------------------------------------------------------------------
// INPUT SCANNER (PRODUCER) — PULSE CH4
// Polls HAL pins and pushes discrete AppEvent tokens into the FIFO queue.
// Still polled in the superloop, so fast encoder turns during ~100 ms OLED
// I2C transfers still drop steps (Part 3 cliffhanger).
// ----------------------------------------------------------------------------
static void ScanInputsToQueue(void) {
  HalGpioSet(kHalPinDbgInput, kHalGpioHigh);

  uint32_t now_ms = HalTimeMsGet();

  // Encoder pushbutton: short press (<1s) vs. long hold (>=1s) for lock toggle
  HalGpioLevel sw_level = HalGpioGet(kHalPinEncSw);
  if (sw_level == kHalGpioLow && app_ctx.last_sw == kHalGpioHigh) {
    app_ctx.sw_press_start_ms     = now_ms;
    app_ctx.sw_long_press_handled = false;
  } else if (sw_level == kHalGpioLow && !app_ctx.sw_long_press_handled) {
    if ((now_ms - app_ctx.sw_press_start_ms) >= APP_LOCK_HOLD_MS) {
      EventQueuePush(kEvtLockToggle);
      app_ctx.sw_long_press_handled = true;
    }
  } else if (sw_level == kHalGpioHigh && app_ctx.last_sw == kHalGpioLow) {
    if (!app_ctx.sw_long_press_handled) {
      EventQueuePush(kEvtEncShortPress);
    }
  }
  app_ctx.last_sw = sw_level;

  // Hardware fault pushbutton (falling edge)
  HalGpioLevel fault_btn_level = HalGpioGet(kHalPinFaultBtn);
  if (fault_btn_level == kHalGpioLow && app_ctx.last_fault_btn == kHalGpioHigh) {
    EventQueuePush(kEvtFaultToggle);
  }
  app_ctx.last_fault_btn = fault_btn_level;

  // Incremental rotary encoder (falling edge on Channel A)
  HalGpioLevel enc_a = HalGpioGet(kHalPinEncA);
  if (enc_a != app_ctx.last_enc_a && enc_a == kHalGpioLow) {
    HalGpioLevel enc_b = HalGpioGet(kHalPinEncB);
    if (enc_b != enc_a) {
      EventQueuePush(kEvtEncCw);
    } else {
      EventQueuePush(kEvtEncCcw);
    }
  }
  app_ctx.last_enc_a = enc_a;

  HalGpioSet(kHalPinDbgInput, kHalGpioLow);
}

// ----------------------------------------------------------------------------
// PUBLIC APPLICATION LIFECYCLE
// ----------------------------------------------------------------------------
void AppInit(void) {
  HalInit();
  DrvLedInit();
  DrvSsd1306Init();

  EventQueueInit();

  app_ctx.current_state         = kAppStateRunning;
  app_ctx.screen_mode           = 0;
  app_ctx.last_enc_a            = HalGpioGet(kHalPinEncA);
  app_ctx.last_sw               = HalGpioGet(kHalPinEncSw);
  app_ctx.last_fault_btn        = HalGpioGet(kHalPinFaultBtn);
  app_ctx.sw_press_start_ms     = 0;
  app_ctx.sw_long_press_handled = false;

  StateOnEnter(app_ctx.current_state);
}

void AppUpdate(void) {
  // Stage 1: Producer — scan hardware inputs into FIFO event queue
  ScanInputsToQueue();

  // Stage 2: Consumer — drain FIFO event queue into FSM dispatcher
  AppEvent evt;
  while (EventQueuePop(&evt)) {
    FsmDispatch(evt);
  }

  // Stage 3: Render LED strip and OLED from settled state
  bool is_output_on = (app_ctx.current_state != kAppStateFault);
  bool is_locked    = (app_ctx.current_state == kAppStateLocked);
  DrvLedFrameRender(is_output_on);
  DrvSsd1306Render(app_ctx.screen_mode, is_output_on, DrvLedBrightnessGet(), is_locked);

  // Stage 4: Superloop delay
  HalDelayMs(20);
}
