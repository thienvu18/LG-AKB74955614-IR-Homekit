#include "ac_state.h"
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <Arduino.h>

/* =========================
 * Forward Declaration
 * ========================= */
void ir_handler_send(void);

/* =========================
 * Canonical Internal State
 * ========================= */
struct ac_state internal_state = {
  .mode = MODE_COOL,
  .active = false,
  .target_temp = 27,
  .fan_speed = 50,
  .light_on = true
};

/* =========================
 * External HomeKit Characteristics
 * (declared in accessory.c)
 * ========================= */
extern homekit_characteristic_t ac_active;
extern homekit_characteristic_t current_heater_cooler_state;
extern homekit_characteristic_t target_heater_cooler_state;
extern homekit_characteristic_t cooling_threshold_temperature;
extern homekit_characteristic_t cooling_rotation_speed;
extern homekit_characteristic_t fan_active;
extern homekit_characteristic_t fan_rotation_speed;
extern homekit_characteristic_t dehumidifier_active;
extern homekit_characteristic_t dehumidifier_rotation_speed;
extern homekit_characteristic_t ac_light;

/* =========================
 * Helper: Notify characteristic if changed
 * ========================= */
static void notify_if_changed(homekit_characteristic_t *chr, homekit_value_t value) {
  if (chr->value.uint8_value != value.uint8_value) {
    chr->value = value;
    homekit_characteristic_notify(chr, value);
  }
}

static void notify_if_changed_float(homekit_characteristic_t *chr, float value) {
  if (chr->value.float_value != value) {
    chr->value = HOMEKIT_FLOAT(value);
    homekit_characteristic_notify(chr, chr->value);
  }
}

static void notify_if_changed_bool(homekit_characteristic_t *chr, bool value) {
  if (chr->value.bool_value != value) {
    chr->value = HOMEKIT_BOOL(value);
    homekit_characteristic_notify(chr, chr->value);
  }
}

/* =========================
 * Sync ALL HomeKit characteristics from internal_state
 * ========================= */
void sync_homekit_characteristics(void) {
  switch (internal_state.mode) {
    case MODE_COOL:
      // HeaterCooler
      notify_if_changed(&ac_active, HOMEKIT_UINT8(internal_state.active ? 1 : 0));
      // HomeKit states: 0=Inactive(not used), 1=Idle, 2=Heating(not used), 3=Cooling
      notify_if_changed(&current_heater_cooler_state,
                        HOMEKIT_UINT8(internal_state.active ? 3 : 1));   // 3=Cooling, 1=Idle
      notify_if_changed(&target_heater_cooler_state, HOMEKIT_UINT8(2));  // 2 = Cool (only valid value)
      notify_if_changed_float(&cooling_threshold_temperature, internal_state.target_temp);
      notify_if_changed_float(&cooling_rotation_speed, internal_state.fan_speed);

      // Turn off other services
      notify_if_changed(&fan_active, HOMEKIT_UINT8(0));
      notify_if_changed(&dehumidifier_active, HOMEKIT_UINT8(0));
      break;

    case MODE_FAN:
      // Fan
      notify_if_changed(&fan_active, HOMEKIT_UINT8(internal_state.active ? 1 : 0));
      notify_if_changed_float(&fan_rotation_speed, internal_state.fan_speed);

      // HeaterCooler to idle
      notify_if_changed(&ac_active, HOMEKIT_UINT8(0));
      notify_if_changed(&current_heater_cooler_state, HOMEKIT_UINT8(1));  // Idle

      // Turn off dehumidifier
      notify_if_changed(&dehumidifier_active, HOMEKIT_UINT8(0));
      break;

    case MODE_DRY:
      // Dehumidifier
      notify_if_changed(&dehumidifier_active, HOMEKIT_UINT8(internal_state.active ? 1 : 0));
      notify_if_changed_float(&dehumidifier_rotation_speed, internal_state.fan_speed);

      // HeaterCooler to idle
      notify_if_changed(&ac_active, HOMEKIT_UINT8(0));
      notify_if_changed(&current_heater_cooler_state, HOMEKIT_UINT8(1));  // Idle

      // Turn off Fan
      notify_if_changed(&fan_active, HOMEKIT_UINT8(0));
      break;
  }

  // Always sync light
  notify_if_changed_bool(&ac_light, internal_state.light_on);
}

/* =========================
 * Initialize State
 * ========================= */
void ac_state_init(void) {
  sync_homekit_characteristics();
}

/* =========================
 * Fan Speed Mapping (AKB74955603 model)
 * Maps 0-100% to LG remote values: 0=Lowest, 1=Low, 2=Medium, 4=High, 5=Auto
 * ========================= */
uint8_t fan_speed_to_lg(uint8_t speed_0_100) {
  // Thresholds chosen for balanced distribution across 5 LG fan levels
  // 0-10%: Lowest (AC barely noticeable), 11-35%: Low, 36-65%: Medium, 66-90%: High, 91-100%: Auto
  if (speed_0_100 <= 10) return 0;  // Lowest (0-10%)
  if (speed_0_100 <= 35) return 1;  // Low (11-35%)
  if (speed_0_100 <= 65) return 2;  // Medium (36-65%)
  if (speed_0_100 <= 90) return 4;  // High (66-90%)
  return 5;                         // Auto (91-100%) - AC decides optimal speed
}

uint8_t fan_speed_from_lg(uint8_t lg_speed) {
  switch (lg_speed) {
    case 0: return 5;    // Lowest → ~5%
    case 1: return 25;   // Low → ~25%
    case 2: return 50;   // Medium → ~50%
    case 4: return 80;   // High → ~80%
    case 5: return 100;  // Auto → ~100%
    default: return 50;  // Unknown → ~50%
  }
}

/* =========================
 * Handle Light Action (for IR commands)
 * ========================= */
void ac_state_handle_light(enum ir_light_action action, enum state_source src) {
  bool previous_light = internal_state.light_on;

  switch (action) {
    case IR_LIGHT_SET_ON:
      internal_state.light_on = true;
      break;
    case IR_LIGHT_SET_OFF:
      internal_state.light_on = false;
      break;
    case IR_LIGHT_TOGGLE:
      internal_state.light_on = !internal_state.light_on;
      break;
  }

  if (previous_light != internal_state.light_on) {
    sync_homekit_characteristics();
  }
}

/* =========================
 * Deferred IR Send Mechanism
 * Collects IR send requests and executes once after all HomeKit events settle
 * ========================= */
static bool ir_send_pending = false;

void ir_request_send(void) {
  ir_send_pending = true;
}

bool ir_send_pending_check(void) {
  if (ir_send_pending) {
    ir_send_pending = false;
    ir_handler_send();
    return true;
  }
  return false;
}

/* =========================
 * Core State Set Function
 * ========================= */
void ac_state_set(enum hvac_mode mode, bool active,
                  uint8_t temp, uint8_t speed, bool light,
                  enum state_source src) {

  // Validate temperature (LG AC range: 16-30°C)
  if (temp < 16) temp = 16;
  if (temp > 30) temp = 30;

  // 1. Update canonical state
  internal_state.mode = mode;
  internal_state.active = active;
  internal_state.target_temp = temp;
  internal_state.fan_speed = speed;
  internal_state.light_on = light;

  // 2. Sync HomeKit characteristics
  sync_homekit_characteristics();

  // 3. Request IR send (deferred to main loop)
  if (src == SOURCE_HOMEKIT) {
    ir_request_send();
  }
}
