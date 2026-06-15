#ifndef _ACCESSORY_H_
#define _ACCESSORY_H_

#include <homekit/characteristics.h>

/* =========================
 * HeaterCooler (Cooling only)
 * ========================= */
extern homekit_characteristic_t ac_active;
extern homekit_characteristic_t current_heater_cooler_state;
extern homekit_characteristic_t target_heater_cooler_state;
extern homekit_characteristic_t cooling_threshold_temperature;  // remove if unsupported
extern homekit_characteristic_t cooling_rotation_speed;         // fan speed for cooling

/* =========================
 * Fan-only
 * ========================= */
extern homekit_characteristic_t fan_active;
extern homekit_characteristic_t fan_rotation_speed;  // optional

/* =========================
 * Dehumidifier (Dry mode)
 * ========================= */
extern homekit_characteristic_t dehumidifier_active;
extern homekit_characteristic_t dehumidifier_rotation_speed;  // fan speed for dry mode

/* =========================
 * Indicator Light
 * ========================= */
extern homekit_characteristic_t ac_light;

#endif  // _ACCESSORY_H_
