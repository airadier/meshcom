/**
 * MeshCom - User Interface
 * Buttons, LED, display (M5StickC Plus 2)
 */
#pragma once

#include "esp_err.h"

typedef enum {
    UI_STATE_IDLE,
    UI_STATE_SHARE,
    UI_STATE_JOIN,
    UI_STATE_BT_DISCO,
} ui_state_t;

/** Initialize UI (buttons, LED, display if available) */
esp_err_t ui_init(void);

/** Set UI state (changes LED pattern and display) */
void ui_set_state(ui_state_t state);

/** Update BT status line on display */
void ui_set_bt_status(const char *status);

/** Update TX/RX indicator */
void ui_set_activity(const char *activity);

/** Flash LED n times (non-blocking) */
void ui_flash_led(int count);
