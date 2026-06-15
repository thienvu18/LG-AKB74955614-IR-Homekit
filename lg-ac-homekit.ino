#include <IRutils.h>
#include "arduino_homekit_server.h"
#include "wifi_info.h"
#include "ac_state.h"
#include "ir_handler.h"
#include <user_interface.h>  // For ESP8266 watchdog and reset info

// Debug mode - disabled for production

/* =========================
 * Hardware Watchdog Configuration
 * ========================= */
#define WATCHDOG_TIMEOUT_MS 5000  // 5 seconds - reset if loop takes longer
static bool watchdog_enabled = false;

/* =========================
 * 24/7 Operation Configuration
 * ========================= */
#define PERIODIC_SYNC_INTERVAL_MS 60000   // 1 minute - sync state to HomeKit
#define HEAP_LOG_INTERVAL_MS 300000       // 5 minutes - log free heap
#define WIFI_RECONNECT_MAX_RETRIES 10     // Max retries before force restart
#define WIFI_RECONNECT_INTERVAL_MS 30000  // 30 seconds between retries
#define MIN_FREE_HEAP_THRESHOLD 15000     // Restart if heap drops below 15KB

static unsigned long wifi_reconnect_count = 0;
static unsigned long last_periodic_sync = 0;
static unsigned long last_heap_log = 0;
static unsigned long boot_time = 0;

/* =========================
 * Watchdog Functions
 * ========================= */
void init_watchdog(void) {
  // Initialize hardware watchdog
  ESP.wdtEnable(WATCHDOG_TIMEOUT_MS);
  watchdog_enabled = true;
  boot_time = millis();
}

void feed_watchdog(void) {
  if (watchdog_enabled) {
    ESP.wdtFeed();
  }
}

/* =========================
 * 24/7 Helper Functions
 * ========================= */

// Check for low memory and restart if critical
void check_memory_threshold(void) {
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < MIN_FREE_HEAP_THRESHOLD) {
    delay(1000);
    ESP.restart();
  }
}

// Periodic tasks for 24/7 reliability
void periodic_tasks(unsigned long now) {
  // Periodic state sync to HomeKit (ensure UI matches reality)
  if (now - last_periodic_sync > PERIODIC_SYNC_INTERVAL_MS) {
    sync_homekit_characteristics();
    last_periodic_sync = now;
  }

  // Periodic serial flush to prevent buffer overflow
  if (now - last_heap_log > HEAP_LOG_INTERVAL_MS) {
    Serial.flush();
    last_heap_log = now;
  }
}

/* =========================
 * Access HomeKit Characteristics
 * (defined in accessory.c)
 * ========================= */
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t ac_active;
extern "C" homekit_characteristic_t ac_light;
extern "C" homekit_characteristic_t current_heater_cooler_state;
extern "C" homekit_characteristic_t target_heater_cooler_state;
extern "C" homekit_characteristic_t cooling_threshold_temperature;
extern "C" homekit_characteristic_t cooling_rotation_speed;
extern "C" homekit_characteristic_t fan_active;
extern "C" homekit_characteristic_t fan_rotation_speed;
extern "C" homekit_characteristic_t dehumidifier_active;
extern "C" homekit_characteristic_t dehumidifier_rotation_speed;

/* =========================
 * IR Objects (defined in ir_handler.c)
 * ========================= */
extern IRLgAc ac;
extern IRrecv irrecv;
decode_results results;

/* =========================
 * HomeKit Setters
 * ========================= */

void ac_active_setter(const homekit_value_t value) {
  // HomeKit ACTIVE: 0=Off, 1=On
  bool on = value.uint8_value == 1;

  if (on) {
    // Power ON: switch to COOL mode, preserve temp/fan, light ON
    ac_state_set(MODE_COOL, true,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 true,  // Light always ON with mode change (LG behavior)
                 SOURCE_HOMEKIT);
  } else {
    // Power OFF: idle in current mode, light OFF
    ac_state_set(internal_state.mode, false,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 false,  // Light OFF
                 SOURCE_HOMEKIT);
  }
}

void target_heater_cooler_state_setter(const homekit_value_t value) {
  // TargetHeaterCoolerState: 0=Auto, 1=Heat, 2=Cool
  // We only support COOL (2) - all other values ignored
  uint8_t state = value.uint8_value;

  if (state == 2) {  // 2 = Cool mode
    ac_state_set(MODE_COOL, internal_state.active,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 true,  // Light ON
                 SOURCE_HOMEKIT);
  }
}

void cooling_threshold_temperature_setter(const homekit_value_t value) {
  // HomeKit sends float (18.0-30.0), LG AC uses uint8_t (16-30)
  float temp = value.float_value;

  // Only valid in COOL mode
  if (internal_state.mode == MODE_COOL) {
    // Round to nearest integer: 24.4->24, 24.5->25 (C standard truncation + 0.5)
    uint8_t rounded_temp = (uint8_t)(temp + 0.5f);
    ac_state_set(MODE_COOL, internal_state.active,
                 rounded_temp,
                 internal_state.fan_speed,
                 true,  // Light turns ON with any command (LG behavior)
                 SOURCE_HOMEKIT);
  }
}

void cooling_rotation_speed_setter(const homekit_value_t value) {
  float speed = value.float_value;

  // Only valid in COOL mode
  if (internal_state.mode == MODE_COOL) {
    ac_state_set(MODE_COOL, internal_state.active,
                 internal_state.target_temp,
                 (uint8_t)speed,
                 true,  // Light turns ON
                 SOURCE_HOMEKIT);
  }
}

void fan_active_setter(const homekit_value_t value) {
  // Fan ACTIVE: 0=Off, 1=On
  bool on = value.uint8_value == 1;

  if (on) {
    // Fan ON: switch to FAN mode
    ac_state_set(MODE_FAN, true,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 true,
                 SOURCE_HOMEKIT);
  } else {
    // Fan OFF: switch to idle COOL (mode exclusivity)
    ac_state_set(MODE_COOL, false,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 false,
                 SOURCE_HOMEKIT);
  }
}

void fan_rotation_speed_setter(const homekit_value_t value) {
  float speed = value.float_value;

  // Only valid in FAN mode
  if (internal_state.mode == MODE_FAN) {
    ac_state_set(MODE_FAN, internal_state.active,
                 internal_state.target_temp,
                 (uint8_t)speed,
                 true,
                 SOURCE_HOMEKIT);
  }
}

void dehumidifier_active_setter(const homekit_value_t value) {
  // Dehumidifier ACTIVE: 0=Off, 1=On
  bool on = value.uint8_value == 1;

  if (on) {
    // DRY ON: switch to DRY mode
    ac_state_set(MODE_DRY, true,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 true,
                 SOURCE_HOMEKIT);
  } else {
    // DRY OFF: switch to idle COOL (mode exclusivity)
    ac_state_set(MODE_COOL, false,
                 internal_state.target_temp,
                 internal_state.fan_speed,
                 false,
                 SOURCE_HOMEKIT);
  }
}

void dehumidifier_rotation_speed_setter(const homekit_value_t value) {
  float speed = value.float_value;

  // Only valid in DRY mode
  if (internal_state.mode == MODE_DRY) {
    ac_state_set(MODE_DRY, internal_state.active,
                 internal_state.target_temp,
                 (uint8_t)speed,
                 true,
                 SOURCE_HOMEKIT);
  }
}

void ac_light_setter(const homekit_value_t value) {
  bool desired_state = value.bool_value;

  // Update internal state
  internal_state.light_on = desired_state;

  // Send full IR command with current mode/temp/fan + light state
  ir_handler_send();

  // Sync ALL HomeKit characteristics (light + current mode state)
  sync_homekit_characteristics();
}

// Note: ac_state_set() is defined in ac_state.c and declared in ac_state.h
// sync_homekit_characteristics() is defined in ac_state.c and declared in ac_state.h

/* =========================
 * Setup
 * ========================= */
void setup() {
  Serial.begin(115200);
  delay(100);  // Wait for serial to stabilize

  // Initialize watchdog early to catch setup hangs
  init_watchdog();

  // Setup Wifi - BLOCK until connected or timeout
  wifi_connect();

  // Verify WiFi is connected (wifi_connect should handle timeout, but double-check)
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    ESP.restart();  // Force restart
  }
  feed_watchdog();  // Reset watchdog after WiFi

  // Setup IR
  ir_handler_init();
  feed_watchdog();

  // Setup HomeKit setters
  ac_active.setter = ac_active_setter;
  ac_light.setter = ac_light_setter;
  target_heater_cooler_state.setter = target_heater_cooler_state_setter;
  cooling_threshold_temperature.setter = cooling_threshold_temperature_setter;
  cooling_rotation_speed.setter = cooling_rotation_speed_setter;
  fan_active.setter = fan_active_setter;
  fan_rotation_speed.setter = fan_rotation_speed_setter;
  dehumidifier_active.setter = dehumidifier_active_setter;
  dehumidifier_rotation_speed.setter = dehumidifier_rotation_speed_setter;

  // Setup HomeKit
  arduino_homekit_setup(&config);
  feed_watchdog();

  // WiFi no-sleep for maximum reliability (higher power, but 24/7 stable)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Initialize HomeKit state from internal state
  ac_state_init();
  feed_watchdog();
}

/* =========================
 * Loop
 * ========================= */
void loop() {
  feed_watchdog();  // Reset watchdog at start of each loop

  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
      wifi_reconnect_count++;

      // Disable watchdog during WiFi reconnect (can take 10+ seconds)
      ESP.wdtDisable();
      wifi_connect();
      ESP.wdtEnable(WATCHDOG_TIMEOUT_MS);

      if (WiFi.status() != WL_CONNECTED) {
        // Force restart after too many failed attempts (stuck in bad state)
        if (wifi_reconnect_count >= WIFI_RECONNECT_MAX_RETRIES) {
          delay(1000);
          ESP.restart();
        }
      } else {
        wifi_reconnect_count = 0;  // Reset counter on success

        // Force sync after reconnection (HomeKit may have missed updates)
        sync_homekit_characteristics();
      }
      lastReconnectAttempt = now;
    }
    // Short delay to prevent tight loop during WiFi issues
    delay(100);
    yield();  // Feed watchdog/WiFi during delay
    return;
  }

  // WiFi is connected
  if (wifi_reconnect_count > 0) {
    wifi_reconnect_count = 0;  // Reset on stable connection
  }
  lastReconnectAttempt = now;

  feed_watchdog();  // Feed after WiFi check

  // HomeKit loop (important to call regularly)
  arduino_homekit_loop();
  feed_watchdog();

  // Process pending IR commands (deferred from setters)
  ir_send_pending_check();
  feed_watchdog();

  // Periodic 24/7 maintenance tasks
  periodic_tasks(now);
  feed_watchdog();

  // Check for memory leaks and restart proactively if needed
  check_memory_threshold();
  feed_watchdog();

  // Check for IR commands from physical remote
  if (irrecv.decode(&results)) {
    feed_watchdog();  // IR decode can take time
    ir_handler_decode(&results);
    irrecv.resume();  // Prepare for the next IR message
    feed_watchdog();
  }

  feed_watchdog();  // Final feed before delay
  yield();          // Feed watchdog/WiFi
  delay(10);
}