#include <IRremoteESP8266.h>
#include <ir_LG.h>
#include "arduino_homekit_server.h"
#include "wifi_info.h"

//access the config defined in C code
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t ac_active;
extern "C" homekit_characteristic_t ac_light;
extern "C" homekit_characteristic_t current_temperature;
extern "C" homekit_characteristic_t current_heater_cooler_state;
extern "C" homekit_characteristic_t target_heater_cooler_state;
extern "C" homekit_characteristic_t cooling_threshold_temperature;

IRLgAc ac(4);

bool commandWaiting = false;

void notify_changed() {
  homekit_characteristic_notify(&ac_active, ac_active.value);
  homekit_characteristic_notify(&ac_light, ac_light.value);
  homekit_characteristic_notify(&current_temperature, current_temperature.value);
  homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
  homekit_characteristic_notify(&target_heater_cooler_state, target_heater_cooler_state.value);
  homekit_characteristic_notify(&cooling_threshold_temperature, cooling_threshold_temperature.value);

  commandWaiting = true;
}

void turn_ac_on() {
  ac.on();
  ac.setMode(kLgAcCool);

  if (cooling_threshold_temperature.value.is_null) {
    ac.setTemp(27);
    current_temperature.value = HOMEKIT_FLOAT(27);
    cooling_threshold_temperature.value = HOMEKIT_FLOAT(27);
  } else {
    float temp = cooling_threshold_temperature.value.float_value;
    ac.setTemp(temp);
    current_temperature.value = HOMEKIT_FLOAT(temp);
  }

  ac_active.value = HOMEKIT_UINT8(1);
  current_heater_cooler_state.value = HOMEKIT_UINT8(3);
  target_heater_cooler_state.value = HOMEKIT_UINT8(2);
}

void turn_ac_off() {
  ac.off();

  ac_active.value = HOMEKIT_UINT8(0);
  current_heater_cooler_state.value = HOMEKIT_UINT8(0);
}

void ac_active_setter(const homekit_value_t value) {
  bool on = value.uint8_value == 1;
  INFO("ac_active: %d\n", on);

  if (on) {
    turn_ac_on();
  } else {
    turn_ac_off();
  }

  notify_changed();
}

void target_heater_cooler_state_setter(const homekit_value_t value) {
  uint8_t state = value.uint8_value;
  INFO("target_heater_cooler_state: %d\n", state);

  if (state != 2) {
    turn_ac_off();
  } else {
    turn_ac_on();
  }

  notify_changed();
}

void cooling_threshold_temperature_setter(const homekit_value_t value) {
  float ctemp = value.float_value;
  INFO("cooling_threshold_temperature: %f\n", ctemp);

  ac.setTemp(ctemp);
  cooling_threshold_temperature.value = HOMEKIT_FLOAT(ctemp);
  current_temperature.value = HOMEKIT_FLOAT(ctemp);

  notify_changed();
}

void ac_light_setter(const homekit_value_t value) {
  uint8_t state = value.uint8_value;
  INFO("ac_light: %f\n", state);

  ac.setLight(state);
  ac_light.value = HOMEKIT_UINT8(state);

  notify_changed();
}

void setup() {
  // Setup Serial
  Serial.begin(115200);

  // Setup Wifi
  wifi_connect();

  // Setup AC IR remote
  ac.setModel(lg_ac_remote_model_t::AKB74955603);
  ac.begin();

  // Setup Homekit
  // homekit_server_reset(); // Always reset for testing purpose
  ac_active.setter = ac_active_setter;
  ac_light.setter = ac_light_setter;
  target_heater_cooler_state.setter = target_heater_cooler_state_setter;
  cooling_threshold_temperature.setter = cooling_threshold_temperature_setter;
  arduino_homekit_setup(&config);

  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
}

void loop() {
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    wifi_connect();
  }

  arduino_homekit_loop();
  yield(); // Feed watchdog timer

  if (commandWaiting) {
    Serial.printf("Send IR...\n");
    ac.send();
    yield(); // Feed watchdog after IR send
    commandWaiting = false;
  }

  delay(10);
}