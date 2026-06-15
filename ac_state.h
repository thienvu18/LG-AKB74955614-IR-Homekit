#ifndef _AC_STATE_H_
#define _AC_STATE_H_

#include <stdint.h>
#include <stdbool.h>

/* =========================
 * HVAC Modes
 * ========================= */
enum hvac_mode {
  MODE_COOL,
  MODE_FAN,
  MODE_DRY
};

/* =========================
 * State Change Source
 * ========================= */
enum state_source {
  SOURCE_HOMEKIT,   // Command from iPhone/HomeKit UI
  SOURCE_IR_REMOTE  // Command decoded from physical remote
};

/* =========================
 * Light Action (for IR commands)
 * ========================= */
enum ir_light_action {
  IR_LIGHT_SET_ON,   // Any mode/temp/fan command → force ON
  IR_LIGHT_SET_OFF,  // Power OFF command → force OFF
  IR_LIGHT_TOGGLE    // Light toggle command → toggle
};

/* =========================
 * Canonical Internal State
 * ========================= */
struct ac_state {
  enum hvac_mode mode;
  bool active;          // true = running, false = idle
  uint8_t target_temp;  // valid only in COOL mode (18-30)
  uint8_t fan_speed;    // 0-100, used in all modes
  bool light_on;        // current light state
};

/* =========================
 * External Variables (defined in main sketch)
 * ========================= */
extern struct ac_state internal_state;

/* =========================
 * State Management Functions
 * ========================= */
void ac_state_init(void);
void ac_state_set(enum hvac_mode mode, bool active,
                  uint8_t temp, uint8_t speed, bool light,
                  enum state_source src);
void ac_state_handle_light(enum ir_light_action action, enum state_source src);

/* =========================
 * IR Send Control Functions
 * ========================= */
void ir_request_send(void);        // Request IR send (deferred)
bool ir_send_pending_check(void);  // Check and send pending IR (call in loop)

/* =========================
 * Fan Speed Mapping (0-100 ↔ LG values)
 * ========================= */
uint8_t fan_speed_to_lg(uint8_t speed_0_100);
uint8_t fan_speed_from_lg(uint8_t lg_speed);

/* =========================
 * HomeKit Sync (called from IR handler)
 * ========================= */
void sync_homekit_characteristics(void);

#endif  // _AC_STATE_H_
