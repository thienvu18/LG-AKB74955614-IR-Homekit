#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <homekit/types.h>

#include "accessory.h"

#define ACCESSORY_NAME ("LG AC")
#define ACCESSORY_SN ("AKB74955614_001")
#define ACCESSORY_MANUFACTURER ("LG")
#define ACCESSORY_MODEL ("AKB74955614")
#define ACCESSORY_FIRMWARE_REVISION ("1.0.0")
#define ACCESSORY_HARDWARE_REVISION ("20250504")

/* =========================
 * HeaterCooler (Cooling only)
 * ========================= */

// Master power
homekit_characteristic_t ac_active =
  HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);

// CurrentHeaterCoolerState: 0=Inactive(not used), 1=Idle, 2=Heating(not used), 3=Cooling
homekit_characteristic_t current_heater_cooler_state =
  HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 0,
                          .min_value = (float[]){ 0 },
                          .max_value = (float[]){ 3 },
                          .valid_values = {
                            .count = 3,
                            .values = (uint8_t[]){ 0, 1, 3 } });

// TargetHeaterCoolerState: 0=Auto, 1=Heat, 2=Cool (we only support Cool)
homekit_characteristic_t target_heater_cooler_state =
  HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 2,
                          .min_value = (float[]){ 2 },
                          .max_value = (float[]){ 2 },
                          .valid_values = {
                            .count = 1,
                            .values = (uint8_t[]){ 2 } });

/*
// Temperature setpoint for cooling mode.
// LG AC supports 16-30°C, we expose 18-30 for safety margin.
// Min step 1.0 = whole degrees only (LG doesn't support half-degree)
 */
homekit_characteristic_t cooling_threshold_temperature =
  HOMEKIT_CHARACTERISTIC_(COOLING_THRESHOLD_TEMPERATURE, 27,
                          .min_value = (float[]){ 18.0 },
                          .max_value = (float[]){ 30.0 },
                          .min_step = (float[]){ 1.0 });

// Fan speed for cooling mode
homekit_characteristic_t cooling_rotation_speed =
  HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 50,
                          .min_value = (float[]){ 0 },
                          .max_value = (float[]){ 100 },
                          .min_step = (float[]){ 1 });

/* =========================
 * Fan-only
 * ========================= */

homekit_characteristic_t fan_active =
  HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);

// Optional — remove if your AC has fixed fan speed
homekit_characteristic_t fan_rotation_speed =
  HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 50,
                          .min_value = (float[]){ 0 },
                          .max_value = (float[]){ 100 },
                          .min_step = (float[]){ 1 });

/* =========================
 * Dehumidifier (Dry mode)
 * ========================= */

homekit_characteristic_t dehumidifier_active =
  HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);

// Fan speed for dry mode
homekit_characteristic_t dehumidifier_rotation_speed =
  HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 50,
                          .min_value = (float[]){ 0 },
                          .max_value = (float[]){ 100 },
                          .min_step = (float[]){ 1 });

/* =========================
 * Indicator Light
 * ========================= */

homekit_characteristic_t ac_light =
  HOMEKIT_CHARACTERISTIC_(ON, 1);

/* =========================
 * Identify
 * ========================= */

void accessory_identify(homekit_value_t _value) {
}

/* =========================
 * Accessories
 * ========================= */

homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1,
                    .category = homekit_accessory_category_air_conditioner,
                    .services = (homekit_service_t *[]){

                      /* Accessory Information */
                      HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                                      .characteristics = (homekit_characteristic_t *[]){
                                        HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
                                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, ACCESSORY_MANUFACTURER),
                                        HOMEKIT_CHARACTERISTIC(MODEL, ACCESSORY_MODEL),
                                        HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
                                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, ACCESSORY_SN),
                                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, ACCESSORY_FIRMWARE_REVISION),
                                        HOMEKIT_CHARACTERISTIC(HARDWARE_REVISION, ACCESSORY_HARDWARE_REVISION),
                                        NULL }),

                      /* Cooling (primary) */
                      HOMEKIT_SERVICE(HEATER_COOLER, .primary = true, .characteristics = (homekit_characteristic_t *[]){ &ac_active, &current_heater_cooler_state, &target_heater_cooler_state,
                                                                                                                         &cooling_threshold_temperature,  // REMOVE if unsupported
                                                                                                                         &cooling_rotation_speed,         // Fan speed
                                                                                                                         NULL }),

                      /* Fan-only */
                      HOMEKIT_SERVICE(FAN2, .characteristics = (homekit_characteristic_t *[]){ &fan_active,
                                                                                               &fan_rotation_speed,  // optional
                                                                                               NULL }),

                      /* Dry mode */
                      HOMEKIT_SERVICE(HUMIDIFIER_DEHUMIDIFIER, .characteristics = (homekit_characteristic_t *[]){ &dehumidifier_active,
                                                                                                                  &dehumidifier_rotation_speed,  // Fan speed
                                                                                                                  NULL }),

                      /* Indicator Light */
                      HOMEKIT_SERVICE(LIGHTBULB, .characteristics = (homekit_characteristic_t *[]){ &ac_light, NULL }),

                      NULL }),
  NULL
};

homekit_server_config_t config = {
  .accessories = accessories,
  .password = "111-11-111"
};
