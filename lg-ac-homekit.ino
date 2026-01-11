#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <ir_LG.h>
#include <IRac.h>
#include <IRutils.h>
#include "arduino_homekit_server.h"
#include "wifi_info.h"

// Debug mode - set to 0 to disable debug printing
#define PRINT_DEBUG 1

// IR Receive configuration
const uint16_t kRecvPin = 14;  // GPIO 14 for IR receiver (D5 on NodeMCU)
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;  // 50ms for A/C protocols

//access the config defined in C code
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t ac_active;
extern "C" homekit_characteristic_t ac_light;
extern "C" homekit_characteristic_t current_temperature;
extern "C" homekit_characteristic_t current_heater_cooler_state;
extern "C" homekit_characteristic_t target_heater_cooler_state;
extern "C" homekit_characteristic_t cooling_threshold_temperature;

IRLgAc ac(4);  // GPIO 4 for IR transmitter (D2 on NodeMCU)
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);

bool commandWaiting = false;
decode_results results;

void notify_changed() {
  homekit_characteristic_notify(&ac_active, ac_active.value);
  homekit_characteristic_notify(&ac_light, ac_light.value);
  homekit_characteristic_notify(&current_temperature, current_temperature.value);
  homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
  homekit_characteristic_notify(&target_heater_cooler_state, target_heater_cooler_state.value);
  homekit_characteristic_notify(&cooling_threshold_temperature, cooling_threshold_temperature.value);

  commandWaiting = true;
}

// Update HomeKit state from received IR command (without triggering IR send)
void update_homekit_from_ir(IRLgAc &receivedAc) {
#if PRINT_DEBUG
  Serial.println("IR received - updating HomeKit state");
#endif

  // Check if AC is on or off
  if (receivedAc.getPower()) {
    // AC is ON
    ac_active.value = HOMEKIT_UINT8(1);
    current_heater_cooler_state.value = HOMEKIT_UINT8(3);  // Cooling
    target_heater_cooler_state.value = HOMEKIT_UINT8(2);   // Cool mode

    // Get temperature from received command
    uint8_t temp = receivedAc.getTemp();
    cooling_threshold_temperature.value = HOMEKIT_FLOAT(temp);
    current_temperature.value = HOMEKIT_FLOAT(temp);

#if PRINT_DEBUG
    Serial.printf("IR: AC ON, Temp: %d\n", temp);
#endif
  } else {
    // AC is OFF
    ac_active.value = HOMEKIT_UINT8(0);
    current_heater_cooler_state.value = HOMEKIT_UINT8(0);  // Inactive

#if PRINT_DEBUG
    Serial.println("IR: AC OFF");
#endif
  }

  // Update our internal AC state to match
  ac.setRaw(receivedAc.getRaw());

  // Notify HomeKit of the changes (but DON'T set commandWaiting)
  homekit_characteristic_notify(&ac_active, ac_active.value);
  homekit_characteristic_notify(&current_temperature, current_temperature.value);
  homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
  homekit_characteristic_notify(&target_heater_cooler_state, target_heater_cooler_state.value);
  homekit_characteristic_notify(&cooling_threshold_temperature, cooling_threshold_temperature.value);

  // DON'T set commandWaiting = true here, as we don't want to send IR back
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
#if PRINT_DEBUG
  INFO("ac_active: %d\n", on);
#endif

  if (on) {
    turn_ac_on();
  } else {
    turn_ac_off();
  }

  notify_changed();
}

void target_heater_cooler_state_setter(const homekit_value_t value) {
  uint8_t state = value.uint8_value;
#if PRINT_DEBUG
  INFO("target_heater_cooler_state: %d\n", state);
#endif

  if (state != 2) {
    turn_ac_off();
  } else {
    turn_ac_on();
  }

  notify_changed();
}

void cooling_threshold_temperature_setter(const homekit_value_t value) {
  float ctemp = value.float_value;
#if PRINT_DEBUG
  INFO("cooling_threshold_temperature: %f\n", ctemp);
#endif

  ac.setTemp(ctemp);
  cooling_threshold_temperature.value = HOMEKIT_FLOAT(ctemp);
  current_temperature.value = HOMEKIT_FLOAT(ctemp);

  notify_changed();
}

void ac_light_setter(const homekit_value_t value) {
  bool state = value.bool_value;
#if PRINT_DEBUG
  INFO("ac_light: %d\n", state);
#endif

  ac.setLight(state);
  ac_light.value = HOMEKIT_BOOL(state);

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

  // Setup IR receive
  irrecv.enableIRIn();  // Start the receiver

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
  static unsigned long lastReconnectAttempt = 0;

  // Check WiFi connection and reconnect if needed (throttled to prevent memory fragmentation)
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 30000) {  // Try once every 30 seconds
#if PRINT_DEBUG
      Serial.println("WiFi disconnected, reconnecting...");
#endif
      wifi_connect();
      lastReconnectAttempt = now;
    }
    delay(10);
  } else {
    arduino_homekit_loop();
    yield();  // Feed watchdog timer

    if (commandWaiting) {
#if PRINT_DEBUG
      Serial.printf("Send IR...\n");
#endif
      ac.send();
      yield();  // Feed watchdog after IR send
      commandWaiting = false;
    }

    delay(10);

    // Check for received IR commands from physical remote
    if (irrecv.decode(&results)) {
      // Check if it's an LG A/C command
      if (results.decode_type == decode_type_t::LG || results.decode_type == decode_type_t::LG2) {
#if PRINT_DEBUG
        Serial.printf("IR Code received: 0x%llX\n", results.value);
#endif

        // Create temporary LG AC object to decode the state
        IRLgAc receivedAc(0);                                             // Dummy pin, we're just using it for decoding
        receivedAc.setRaw((uint32_t)results.value, results.decode_type);  // Use results.value as uint32_t

        // Update HomeKit state without sending IR back
        update_homekit_from_ir(receivedAc);
      }

      yield();
      irrecv.resume();  // Prepare for the next IR message
    }
  }
}