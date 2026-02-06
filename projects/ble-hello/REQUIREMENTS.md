# Project Requirements

**Last Updated**: 2026-02-06
**Project**: BLE HID Media Remote
**Hardware**: nRF54L15 Development Kit (nRF54L15-DK)

---

## Project Overview
NFC-triggered BLE HID Consumer Control device (media remote). BLE advertising is off by default and activated by tapping an NFC-enabled phone on the device, which starts a 30-second advertising window. Once paired, the 4 buttons act as media controls (volume up/down, play/pause, next track) via HID over GATT.

## Hardware Requirements
- nRF54L15 Development Kit (PCA10156)
- USB cable (for programming and power)
- Smartphone with Bluetooth (iPhone or Android) for HID pairing

## Functional Requirements

### FR-001: NFC-Triggered BLE Advertising
**Priority**: High
**Status**: Implemented
**Description**: BLE advertising is controlled by NFC field detection instead of starting on boot
**Acceptance Criteria**:
- [x] No BLE advertising on boot (NFC T2T emulation starts instead)
- [x] Phone NFC tap triggers 30-second BLE advertising window
- [x] Device appears in BLE scanner as "nRF54L15_Hello"
- [x] Connectable and scannable
- [x] Advertising stops automatically after 30s if no connection
- [x] Subsequent NFC taps restart advertising (stop/start cycle for iOS compatibility)
- [x] NFC tag presents NDEF text record readable by phone

### FR-002: LED Status Indication
**Priority**: High
**Status**: Implemented
**Description**: LEDs indicate NFC detection and BLE connection state
**Acceptance Criteria**:
- [x] LED1 off when idle
- [x] LED1 blinks while advertising (not connected)
- [x] LED1 solid when connected
- [x] LED2 blinks 5 times on NFC field detection

### FR-003: HID Consumer Control (Media Remote)
**Priority**: High
**Status**: Implemented
**Description**: 4 buttons send HID Consumer Control reports over BLE for media/volume control
**Acceptance Criteria**:
- [x] Button1 sends Volume Up (HID usage 0x00E9)
- [x] Button2 sends Volume Down (HID usage 0x00EA)
- [x] Button3 sends Play/Pause (HID usage 0x00CD)
- [x] Button4 sends Next Track (HID usage 0x00B5)
- [x] HID reports include key press followed by key release
- [x] BLE encryption enabled (required by iOS for HID)
- [x] Bond storage via NVS settings for persistent pairing
- [x] Overwrite oldest bond key when storage full
- [x] Hold Button4 at boot to clear all bonds (LED2 blinks 3x to confirm)
- [x] HID, BAS, and DIS services advertised

## Technical Specifications

### Pin Configuration (nRF54L15-DK)
```
LED1  -> P1.10 (Active low)  - BLE status
LED2  -> P1.14 (Active low)  - NFC blink

Button1 -> P1.13 (Active low, internal pull-up) - Volume Up
Button2 -> P1.09 (Active low, internal pull-up) - Volume Down
Button3 -> P1.08 (Active low, internal pull-up) - Play/Pause
Button4 -> P0.04 (Active low, internal pull-up) - Next Track
```

### BLE Configuration
```
Device Name: nRF54L15_Hello
Appearance: 961 (HID Generic)
TX Power: 0 dBm
Advertising Interval: 100ms
Connection Interval: 15-30ms
Services: HID (0x1812), Battery (0x180F), Device Info (0x180A)
Security: SMP with bonding (NVS-backed)
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
- [x] Verify no BLE advertising on boot
- [x] Verify NFC tap starts BLE advertising (device appears in nRF Connect app)
- [x] Verify LED2 blinks 5x on NFC detection
- [x] Verify LED1 blinks during advertising
- [ ] Verify pairing completes from iPhone Settings → Bluetooth
- [ ] Verify LED1 solid when connected
- [x] Verify advertising stops after 30s timeout
- [x] Verify re-tap starts advertising again
- [ ] Verify Button1 → iPhone volume up
- [ ] Verify Button2 → iPhone volume down
- [ ] Verify Button3 → music play/pause
- [ ] Verify Button4 → next track
- [x] Check serial/RTT output for debug messages

## Development Notes
- iOS caches BLE bond keys aggressively; if pairing fails with "Security failed: err 4", clear bonds on both device (Button4+Reset) and iPhone (Forget Device or restart phone)
- Advertising must be stopped and restarted (not just continued) on repeated NFC taps for iOS to detect it reliably

---

## Change Log
- 2026-02-05: Initial requirements created
- 2026-02-05: Added NFC-triggered BLE advertising (FR-001 rewritten, FR-005 merged in)
- 2026-02-05: Added LED2 blink 5x on NFC detection
- 2026-02-06: Added HID Consumer Control (FR-003), removed custom GATT service (FR-004), 4-button media remote
- 2026-02-06: Fix iOS pairing: add bond clearing (Button4+boot), restart advertising on NFC re-tap, overwrite oldest bonds
