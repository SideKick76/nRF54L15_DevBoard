# Project Requirements

**Last Updated**: 2026-02-05
**Project**: BLE Hello World
**Hardware**: nRF54L15 Development Kit (nRF54L15-DK)

---

## Project Overview
Basic Bluetooth Low Energy peripheral to verify hardware setup and BLE stack functionality.

## Hardware Requirements
- nRF54L15 Development Kit (PCA10156)
- USB cable (for programming and power)
- Smartphone with nRF Connect app (for testing)

## Functional Requirements

### FR-001: BLE Advertising
**Priority**: High
**Description**: Device advertises as a connectable BLE peripheral
**Acceptance Criteria**:
- [ ] Device appears in BLE scanner as "nRF54L15_Hello"
- [ ] Advertising interval: 100ms
- [ ] Connectable and scannable

### FR-002: LED Status Indication
**Priority**: High
**Description**: LEDs indicate BLE connection state
**Acceptance Criteria**:
- [ ] LED1 blinks while advertising (not connected)
- [ ] LED1 solid when connected
- [ ] LED2 toggles on button press

### FR-003: Button Interaction
**Priority**: Medium
**Description**: Button triggers BLE notification
**Acceptance Criteria**:
- [ ] Button1 press sends notification to connected central
- [ ] Debounce handling (50ms)

### FR-004: Custom BLE Service
**Priority**: Medium
**Description**: Implement simple custom GATT service
**Acceptance Criteria**:
- [ ] Custom service UUID: define in code
- [ ] Read characteristic: returns button press count
- [ ] Notify characteristic: sends on button press

## Technical Specifications

### Pin Configuration (nRF54L15-DK)
```
LED1  -> P1.10 (Active low)
LED2  -> P1.14 (Active low)
LED3  -> P2.09 (Active low)
LED4  -> P2.10 (Active low)

Button1 -> P1.13 (Active low, internal pull-up)
Button2 -> P1.09 (Active low, internal pull-up)
Button3 -> P2.09 (Active low, internal pull-up)
Button4 -> P0.02 (Active low, internal pull-up)
```

### BLE Configuration
```
Device Name: nRF54L15_Hello
TX Power: 0 dBm
Advertising Interval: 100ms
Connection Interval: 15-30ms
```

### Build Configuration
```
Board: nrf54l15dk/nrf54l15/cpuapp
SDK: nRF Connect SDK v2.7.0+
Build System: CMake + west
```

## Project Structure
```
ble-hello/
├── REQUIREMENTS.md
├── CMakeLists.txt
├── prj.conf
├── src/
│   └── main.c
└── boards/
    └── nrf54l15dk_nrf54l15_cpuapp.overlay  (optional)
```

## Testing Requirements
- [ ] Verify device appears in nRF Connect app
- [ ] Verify connection establishes successfully
- [ ] Verify LED states match connection status
- [ ] Verify button press sends notification
- [ ] Check serial output for debug messages

## Development Notes
[Add notes during development]

---

## Change Log
- 2026-02-05: Initial requirements created
