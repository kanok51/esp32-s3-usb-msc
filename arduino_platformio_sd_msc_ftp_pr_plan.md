# PR Plan for Arduino + PlatformIO ESP32-S3 SD / Read/write MSC / FTP / Web UI Project

This PR plan is based on the simplified Arduino architecture:
- one SD filesystem
- Read/write MSC
- FTP on same filesystem
- web UI controls
- persisted service states

## PR 1 — PlatformIO + Arduino project skeleton

### Goal
Create the base repository structure and confirm the board boots under PlatformIO.

### Scope
- Add `platformio.ini`
- Add minimal `src/main.cpp`
- Add `include/config.h`
- Add boot banner
- Confirm build/upload/monitor workflow

### Acceptance
- `pio run` succeeds
- `pio run -t upload` succeeds
- serial monitor shows boot banner

---

## PR 2 — SD card module

### Goal
Initialize the microSD card reliably over SPI.

### Scope
- Add `sd_card.h/.cpp`
- Use known-good pins:
  - GPIO18 SCK
  - GPIO16 MISO
  - GPIO17 MOSI
  - GPIO15 CS
- Detect card
- Log card type and size

### Acceptance
- SD initializes reliably
- missing-card error is clear in serial log

---

## PR 3 — App state model

### Goal
Add shared runtime state and last-error tracking.

### Scope
- Add `app_state.h/.cpp`
- Track:
  - SD ready
  - MSC enabled
  - FTP enabled
  - Wi-Fi connected
  - last error
- Provide helpers to update/query state

### Acceptance
- core modules can share common state cleanly

---

## PR 4 — Settings persistence

### Goal
Persist service enable flags across reboot.

### Scope
- Add `settings_store.h/.cpp`
- Use Arduino Preferences / NVS-backed storage
- Save/load:
  - MSC enabled flag
  - FTP enabled flag
- Safe fallback to both disabled

### Acceptance
- service states survive reboot
- restore failure falls back safely

---

## PR 5 — Read/write MSC module

### Goal
Expose the SD card to the host as Read/write USB MSC.

### Scope
- Add `usb_msc_sd.h/.cpp`
- Implement:
  - begin MSC
  - end MSC
  - refresh MSC
  - read callback
  - write callback
- Keep logs for enable/disable/refresh

### Acceptance
- host PC sees drive
- host can read files
- host can write files

---

## PR 6 — FTP service module

### Goal
Run FTP on the SD filesystem.

### Scope
- Add `ftp_service.h/.cpp`
- Start/stop FTP server
- Point FTP root to SD filesystem
- Keep disabled by default
- Report state to app state

### Acceptance
- FTP client can connect
- FTP upload/download works

---

## PR 7 — Web UI skeleton

### Goal
Create a small web status page and API surface.

### Scope
- Add `web_ui.h/.cpp`
- Show:
  - IP address
  - SD status
  - MSC state
  - FTP state
  - last error
- Add `/api/status`

### Acceptance
- browser can open status page
- JSON status endpoint works

---

## PR 8 — Web UI MSC controls

### Goal
Control MSC from browser.

### Scope
- Add actions/endpoints:
  - Enable MSC
  - Disable MSC
  - Refresh MSC
- Update persisted settings when MSC is enabled/disabled
- Block enable when SD missing

### Acceptance
- MSC can be controlled from web UI
- Refresh MSC works
- state survives reboot

---

## PR 9 — Web UI FTP controls

### Goal
Control FTP from browser.

### Scope
- Add actions/endpoints:
  - Enable FTP
  - Disable FTP
- Update persisted settings when FTP is enabled/disabled
- Block enable when SD missing or Wi-Fi unavailable

### Acceptance
- FTP can be controlled from web UI
- state survives reboot

---

## PR 10 — Developer test scripts and workflow docs

### Goal
Make the project easy to validate from a development PC.

### Scope
- Add/update Python FTP upload script
- Document:
  - PlatformIO upload commands
  - monitor usage
  - manual reset hints
  - FTP test flow
  - MSC host test flow
- Add serial-log expectations for key actions

### Acceptance
- another engineer can flash and test the device from docs/scripts

---

## PR 11 — Stability polish for shared MSC/FTP filesystem

### Goal
Make the one-partition workflow understandable and stable.

### Scope
- Improve logs around:
  - MSC enable/disable
  - MSC refresh
  - FTP uploads
- Add clear UI note:
  - after FTP upload, host may need MSC refresh
- Prevent confusing or unsafe state transitions where possible

### Acceptance
- expected host refresh workflow is documented and visible
- user can reliably upload via FTP and then refresh MSC

---

## PR 12 — Optional format action and maintenance tools

### Goal
Add maintenance actions if still needed.

### Scope
- Optional web action to format SD
- confirmation guard
- clear warnings
- maintenance/help page

### Acceptance
- destructive actions are guarded and understandable

---

## Suggested merge order

1. PR 1 — Project skeleton
2. PR 2 — SD card module
3. PR 3 — App state
4. PR 4 — Settings persistence
5. PR 5 — Read/write MSC module
6. PR 6 — FTP service
7. PR 7 — Web UI skeleton
8. PR 8 — Web UI MSC controls
9. PR 9 — Web UI FTP controls
10. PR 10 — Test scripts and workflow docs
11. PR 11 — Stability polish
12. PR 12 — Optional maintenance tools

---

## Notes for implementation agents

- Stay on **Arduino + PlatformIO**
- Do not migrate to ESP-IDF in this phase
- Preserve the already working SD/MSC/FTP direction
- Assume the host may require **Refresh MSC** after FTP uploads
- Use serial logs, Python FTP tests, and host-side MSC checks as standard validation
