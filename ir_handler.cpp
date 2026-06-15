#include "ir_handler.h"
#include "ac_state.h"
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <ir_LG.h>
#include <IRutils.h>
#include <Arduino.h>

/* =========================
 * IR Configuration
 * ========================= */
const uint16_t kRecvPin = 14;  // GPIO 14 for IR receiver (D5 on NodeMCU)
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;  // 50ms for A/C protocols

// IR objects
IRLgAc ac(4);  // GPIO 4 for IR transmitter (D2 on NodeMCU)
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);

/* =========================
 * TX-RX Echo Prevention
 * IR LED bounces back to receiver - ignore during TX and brief ACK window
 * ========================= */
static unsigned long ir_tx_time = 0;
static bool ir_tx_active = false;
#define IR_TX_DONE_IGNORE_MS 200  // Ignore echo briefly after TX completes

/* =========================
 * External AC State
 * ========================= */
extern struct ac_state internal_state;

/* =========================
 * Send IR Command based on internal_state
 * ========================= */
void ir_handler_send(void) {
  const char* mode_name;

  if (internal_state.active) {
    // AC is running - send normal command
    ac.on();
    ac.setLight(internal_state.light_on);

    switch (internal_state.mode) {
      case MODE_COOL:
        ac.setMode(kLgAcCool);
        ac.setTemp(internal_state.target_temp);
        ac.setFan(fan_speed_to_lg(internal_state.fan_speed));
        mode_name = "COOL";
        break;

      case MODE_FAN:
        ac.setMode(kLgAcFan);
        ac.setFan(fan_speed_to_lg(internal_state.fan_speed));
        mode_name = "FAN";
        break;

      case MODE_DRY:
        ac.setMode(kLgAcDry);
        ac.setFan(fan_speed_to_lg(internal_state.fan_speed));
        mode_name = "DRY";
        break;

      default:
        mode_name = "UNKNOWN";
    }

    Serial.printf("[IR TX] ON: mode=%s, temp=%d, fan=%d, light=%d\n",
                  mode_name, internal_state.target_temp,
                  internal_state.fan_speed, internal_state.light_on);
  } else {
    // AC is idle - send power OFF command
    ac.off();
    Serial.println("[IR TX] OFF");
  }

  ir_tx_active = true;
  ac.send();
  ir_tx_time = millis();
  ir_tx_active = false;
}

/* =========================
 * Decode IR Command from Remote
 * Returns true if successfully decoded
 * ========================= */
bool ir_handler_decode(decode_results* results) {
  // Ignore RX briefly after TX to prevent hardware echo
  if (ir_tx_active || millis() - ir_tx_time < IR_TX_DONE_IGNORE_MS) {
    return false;  // Likely our own TX echo
  }

  Serial.printf("[IR RX] Received: decode_type=%d, value=0x%08lX\n",
                results->decode_type, results->value);

  // Check if it's an LG A/C command
  if (results->decode_type != decode_type_t::LG && results->decode_type != decode_type_t::LG2) {
    Serial.printf("[IR RX] Rejected: not LG protocol (got type %d)\n", results->decode_type);
    return false;
  }

  // Create temporary LG AC object to decode the state
  IRLgAc receivedAc(0);  // Dummy pin for decoding
  receivedAc.setRaw((uint32_t)results->value, results->decode_type);

  // Check if it's a light toggle command
  if (receivedAc.isLightToggle()) {
    Serial.println("[IR RX] Light toggle detected");
    internal_state.light_on = !internal_state.light_on;
    sync_homekit_characteristics();
    Serial.printf("[IR RX] Light state: %d\n", internal_state.light_on);
    return true;
  }

  // Check if AC is on or off
  if (receivedAc.getPower()) {
    // AC is ON
    uint8_t mode = receivedAc.getMode();
    bool received_light = receivedAc.getLight();

    switch (mode) {
      case kLgAcCool:
        {
          internal_state.mode = MODE_COOL;
          internal_state.active = true;

          // LG AC valid range: 16-30°C, clamp to prevent garbage values
          uint8_t recv_temp = receivedAc.getTemp();
          if (recv_temp < 16) recv_temp = 16;  // Too low = treat as minimum
          if (recv_temp > 30) recv_temp = 30;  // Too high = treat as maximum
          internal_state.target_temp = recv_temp;

          internal_state.fan_speed = fan_speed_from_lg(receivedAc.getFan());
          internal_state.light_on = received_light;  // Use light state from remote
          break;
        }  // end case kLgAcCool

      case kLgAcFan:
        internal_state.mode = MODE_FAN;
        internal_state.active = true;
        internal_state.fan_speed = fan_speed_from_lg(receivedAc.getFan());
        internal_state.light_on = received_light;
        break;

      case kLgAcDry:
        internal_state.mode = MODE_DRY;
        internal_state.active = true;
        internal_state.fan_speed = fan_speed_from_lg(receivedAc.getFan());
        internal_state.light_on = received_light;
        break;

      default:
        return false;
    }

  } else {
    internal_state.active = false;
    internal_state.light_on = false;
  }

  // Sync HomeKit with new state
  sync_homekit_characteristics();

  Serial.printf("[IR RX] Decoded OK: active=%d, mode=%d, temp=%d, fan=%d, light=%d\n",
                internal_state.active, internal_state.mode,
                internal_state.target_temp, internal_state.fan_speed,
                internal_state.light_on);

  return true;
}

/* =========================
 * Initialize IR Handler
 * ========================= */
void ir_handler_init(void) {
  // Setup AC IR remote
  ac.setModel(lg_ac_remote_model_t::AKB74955603);
  ac.begin();

  irrecv.enableIRIn();
}
