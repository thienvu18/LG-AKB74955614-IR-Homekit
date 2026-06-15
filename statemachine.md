# HomeKit IR AC Controller — State Machine & Architecture

This document defines the **authoritative behavior** for a **sensorless**, **cooling-focused** AC  
controlled via **bi-directional IR**, integrated with **Apple HomeKit**.

---

## 1. Hardware & Reality Assumptions

| Assumption | Value |
|------------|-------|
| AC Modes | **Cool / Fan / Dry** |
| Temperature Sensors | **None** (sensorless) |
| AC Power | **Always on** |
| "Off" Meaning | **Idle** (not unavailable) |

### 1.1 IR Capabilities
- **TX**: Send commands to AC
- **RX**: Decode physical remote commands (best-effort)
- **Echo Prevention**: TX LED bounce filtered for 200ms after TX

---

## 2. Mode Architecture

### 2.1 Exclusivity Rule
```
┌─────────────────────────────────────────────┐
│  COOL (HeaterCooler)                        │
│       OR                                     │
│  FAN (Fanv2)                                │
│       OR                                     │
│  DRY (Dehumidifier)                         │
└─────────────────────────────────────────────┘
```
Only **one mode** can be active at a time. Activating one automatically deactivates others.

### 2.2 Mode ↔ Service Mapping

| Mode | HomeKit Service | Active Char | Speed Char |
|------|----------------|-------------|------------|
| COOL | HeaterCooler | `ac_active` | `cooling_rotation_speed` |
| FAN | Fanv2 | `fan_active` | `fan_rotation_speed` |
| DRY | Dehumidifier | `dehumidifier_active` | `dehumidifier_rotation_speed` |

---

## 3. Light Behavior

### 3.1 Light Control
Controlled via separate **Lightbulb** service (`ac_light`).

### 3.2 IR Command Effects on Light

| Command Type | Light Result |
|-------------|-------------|
| Mode change (Cool/Fan/Dry) | **ON** |
| Temperature change | **ON** |
| Fan speed change | **ON** |
| **Light toggle** (remote) | **Toggle ON↔OFF** |
| **Power OFF** | **OFF** |

> **Note**: Most IR commands force light **ON**. Only Light Toggle and Power OFF have different behavior.

---

## 4. HomeKit State Values

### 4.1 HeaterCooler States (CurrentHeaterCoolerState)

| Value | Name | Meaning |
|-------|------|---------|
| 0 | Inactive | **Never used** (AC is always available) |
| 1 | Idle | Mode active but not cooling |
| 2 | Heating | Not used |
| 3 | Cooling | Active and cooling |

### 4.2 TargetHeaterCoolerState

| Value | Name | Meaning |
|-------|------|---------|
| 2 | Cool | **Only value allowed** |

### 4.3 Fan/Dehumidifier Active

| Value | Meaning |
|-------|---------|
| 0 | Off/Idle |
| 1 | On/Active |

---

## 5. Fan Speed Mapping

### 5.1 LG AC Remote Values (AKB74955603)
| LG Value | Name | Internal (%) |
|----------|------|---------------|
| 0 | Lowest | 5% |
| 1 | Low | 25% |
| 2 | Medium | 50% |
| 4 | High | 80% |
| 5 | Auto | 100% |

### 5.2 Threshold Mapping (0-100 → LG)
| Range | LG Value |
|-------|----------|
| 0-10% | 0 (Lowest) |
| 11-35% | 1 (Low) |
| 36-65% | 2 (Medium) |
| 66-90% | 4 (High) |
| 91-100% | 5 (Auto) |

---

## 6. Temperature Constraints

| Parameter | Value |
|----------|-------|
| Min | 16°C |
| Max | 30°C |
| Default | 27°C |
| Step | 1°C |

---

## 7. Event Flow Architecture

### 7.1 HomeKit → AC (Command Flow)
```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  HomeKit     │────▶│  Setter      │────▶│ ac_state_set │
│  UI Event    │     │  (one per    │     │  - Update    │
│              │     │   char)      │     │    state     │
└──────────────┘     └──────────────┘     │  - Sync HK   │
                                           │  - Request   │
                                           │    IR send  │
                                           └──────┬───────┘
                                                  │ pending flag
                                                  ▼
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│     AC       │◀────│  ir_handler  │◀────│ Main Loop    │
│   (IR TX)   │     │   _send()    │     │ ir_send_     │
│             │     │              │     │ pending_check │
└──────────────┘     └──────────────┘     └──────────────┘
```

**Key Design**: IR is **deferred** to main loop to collect all setter changes into single command.

### 7.2 Remote → AC → HomeKit (Feedback Flow)
```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Physical   │────▶│ ir_handler   │────▶│ sync_homekit │
│   Remote     │     │  _decode()   │     │ _character- │
│   (IR RX)    │     │  - Parse LG  │     │   istics()   │
└──────────────┘     │  - Validate  │     └──────────────┘
                      │  - Update    │
                      │    state    │
                      └──────────────┘
```

---

## 8. Echo Prevention

### 8.1 TX-RX Hardware Echo
IR TX LED bounces off surfaces and can be picked up by RX sensor.

### 8.2 Solution
```
TX starts ──▶ TX ends ──▶ [200ms ignore window] ──▶ RX enabled
```
- `ir_tx_active` flag during TX burst
- `ir_tx_time` timestamp for post-TX window

---

## 9. Deferred IR Send Mechanism

### 9.1 Problem
HomeKit can fire multiple setters for one user action:
1. `ac_active_setter(ON)`
2. `target_heater_cooler_state_setter(COOL)`
3. `cooling_threshold_temperature_setter(25)`

Each setter would trigger IR if sent immediately.

### 9.2 Solution
```
Setter called
    │
    ▼
ac_state_set()
    │
    ├── Update internal_state
    ├── sync_homekit_characteristics()
    └── ir_request_send() ──▶ sets ir_send_pending = true
                                    (no IR sent yet)

Main Loop (after all setters complete)
    │
    ▼
ir_send_pending_check()
    │
    └── ir_handler_send() ──▶ sends ONE IR with final state
```

---

## 10. Source Attribution

| Source | Constant | Behavior |
|--------|----------|----------|
| HomeKit UI | `SOURCE_HOMEKIT` | Triggers IR send |
| Physical Remote | `SOURCE_IR_REMOTE` | Updates state, syncs HomeKit |

---

## 11. Initial State

| Variable | Default |
|----------|---------|
| mode | MODE_COOL |
| active | false |
| target_temp | 27 |
| fan_speed | 50 |
| light_on | true |

---

## 12. File Architecture

| File | Purpose |
|------|---------|
| `accessory.c` | HomeKit accessory definition (services, characteristics) |
| `ac_state.cpp/h` | Canonical state, sync logic, IR deferral |
| `ir_handler.cpp/h` | IR TX/RX, LG protocol, echo prevention |
| `lg-ac-homekit.ino` | Setup, loop, setters, watchdog |

---

## 13. Debug Logging

### 13.1 IR TX
```
[IR TX] ON: mode=COOL, temp=24, fan=50, light=1
[IR TX] OFF
```

### 13.2 IR RX
```
[IR RX] Received: decode_type=XX, value=0xXXXXXXXX
[IR RX] Rejected: not LG protocol (got type XX)
[IR RX] Light toggle detected
[IR RX] Decoded OK: active=1, mode=0, temp=24, fan=2, light=1
```
