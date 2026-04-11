# Arduino + PlatformIO Requirements for ESP32-S3 SD / Read/write MSC / FTP / Web UI

## 1. Purpose

Build firmware for **ESP32-S3** using the **Arduino framework** and **PlatformIO** that uses a **SPI-connected microSD card** and provides:

- a **USB MSC device** exposing the SD card to a host PC as **Read/write**
- an **FTP server** that reads and writes files on the same SD card filesystem
- a **web UI** for system control and status
- persistent settings so MSC/FTP enable states survive reboot
- a practical developer workflow for flashing, reset, logging, FTP testing, and MSC testing

This document is intended so any implementation agent can understand the target architecture and implement it feature-by-feature.

---

## 2. Core design decision

This project shall use:

- **Arduino framework**
- **PlatformIO**
- **one FAT filesystem on the SD card**
- **MSC exported as Read/write**
- **FTP operating on the same filesystem**
- **web UI to enable/disable MSC and FTP**
- **web UI action to refresh/re-enumerate MSC**
- persistent settings across reboot

This is the required architecture for the current phase of the project.

## 2.1. Example Code

Agents should follow example code for MSC, FTP, SD_CARD from working_example/src folder and example platform.ini from working_example directory.

---

## 3. Why this architecture

A one-partition design is acceptable here because:

- the current Arduino codebase already has working SD, MSC, and FTP functionality
- this architecture is simpler and faster to implement than a two-partition direct-FatFs ESP-IDF design

Known limitation:

- when FTP uploads or changes files, the host PC may not immediately see the updated directory contents while MSC is already mounted
- therefore a **Refresh MSC** action is required in the web UI
- refresh behavior should detach and re-enable MSC so the host sees an updated view

---

## 4. Development environment requirement

The project shall be implemented using:

- **Arduino framework**
- **PlatformIO** as the build, upload, and monitor environment

Implementation agents should assume:

- the repository is structured for **PlatformIO + Arduino**
- `platformio.ini` is the primary build entry point
- `src/` is the default source directory unless explicitly changed

## 5. Hardware assumptions

## 5.1 Board
- ESP32-S3 board compatible with native USB device mode
- Existing known working target: YD-ESP32-S3 / VCC-GND style board

## 5.2 SD card wiring
Use the known-good SPI pin set:

- `SCK  = GPIO18`
- `MISO = GPIO16`
- `MOSI = GPIO17`
- `CS   = GPIO15`

## 5.3 USB
- MSC must use the **native USB device connection** of the ESP32-S3
- development PC will be used both as:
  - serial log host
  - MSC host
  - FTP test client

---

## 6. Storage architecture

## 6.1 Physical storage
- one microSD card
- one normal FAT filesystem on the card
- Arduino `SD` library used for filesystem access

## 6.2 Filesystem model
- the SD card shall contain one normal FAT filesystem
- FTP shall read/write files on that filesystem
- MSC shall expose that same filesystem to the host as **Read/write**

## 6.3 Ownership model
- host PC is allowed to write through MSC
- firmware may write through FTP and local code
- firmware must be aware that the host may not immediately detect file updates
- therefore **Refresh MSC** is a required action

---

## 7. Required runtime behavior

## 7.1 Default power-on behavior
On boot:

- initialize serial logging
- initialize SD if present
- initialize Wi-Fi if configured
- **MSC must be disabled by default**
- **FTP must be disabled by default**
- web UI should be available if Wi-Fi is available
- saved settings must be loaded from persistent storage
- if restore fails, both MSC and FTP must remain disabled

## 7.2 MSC behavior
- MSC can be enabled or disabled via web UI
- MSC shall be exported as **Read/write**
- host writes must modify the card
- when disabling MSC:
  - perform best-effort clean detach
- when enabling MSC:
  - ensure SD is mounted and card is present
- provide a **Refresh MSC** action:
  - disable MSC
  - short delay
  - re-enable MSC

## 7.3 FTP behavior
- FTP can be enabled or disabled via web UI
- FTP operates on the same SD filesystem
- FTP is allowed to upload, download, list, delete, mkdir, rmdir if supported by the chosen library
- FTP should not require MSC to be disabled for operation
- however, UI must clearly state that the host may need MSC refresh to see new files

## 7.4 Web UI behavior
The web UI shall allow:
- enabling MSC
- disabling MSC
- refreshing MSC
- enabling FTP
- disabling FTP
- optionally formatting SD later
- viewing status and last error

---

## 8. Persistence requirements

The following settings shall persist across reboot using non-volatile storage:

- MSC enabled/disabled
- FTP enabled/disabled
- Wi-Fi configuration if added to UI later
- any future service enable flags

Recommended storage:
- Arduino `Preferences` / ESP32 NVS-backed storage

Safe fallback:
- if settings cannot be restored, keep MSC disabled and FTP disabled

---

## 9. Web UI requirements

## 9.1 Minimum status information
Web UI should display:

- device IP address
- SD card detected or not
- SD card size
- MSC state: enabled/disabled
- FTP state: enabled/disabled
- last error message if any
- brief note that host refresh may be needed after FTP uploads

## 9.2 Minimum actions
Buttons or endpoints:

- Enable MSC
- Disable MSC
- Refresh MSC
- Enable FTP
- Disable FTP
- Refresh status

Optional later:
- Format SD
- File listing
- Upload/download via browser

## 9.3 UI safety
- if SD card is missing, block MSC enable and FTP enable
- if an operation fails, show a clear error
- destructive actions later must require confirmation

---

## 10. MSC implementation requirements

## 10.1 USB behavior
MSC implementation shall use Arduino ESP32-S3 USB MSC support.

Required capabilities:
- begin MSC
- end MSC
- set media present state
- read callback for sectors
- write callback for sectors


Goal:
- the development PC must be able to browse and copy files from the drive
- the host must be able to modify files on the SD card via MSC

## 10.3 Refresh MSC behavior
A dedicated function shall:
- disable MSC cleanly
- delay briefly
- re-enable MSC

Purpose:
- help the host PC see files newly uploaded through FTP

---

## 11. FTP implementation requirements

- use an Arduino-compatible FTP server library
- FTP root shall point to the SD card filesystem
- FTP must be disabled by default
- FTP enable state must persist across reboot
- FTP service status must be visible from the web UI

Developer note:
- the previously working Python FTP upload script should remain a valid test tool

---

## 12. Error handling requirements

The firmware must not silently fail.

At minimum:
- important failures logged to serial
- latest error visible in web UI
- operations rejected safely if prerequisites are missing

Examples:
- SD card missing
- MSC start failed
- MSC refresh failed
- FTP start failed
- Wi-Fi not connected
- settings restore failed

---

## 13. Recommended module breakdown

Recommended repository layout:

```text
project/
├── platformio.ini
├── include/
│   └── config.h
├── src/
│   ├── main.cpp
│   ├── sd_card.h
│   ├── sd_card.cpp
│   ├── usb_msc_sd.h
│   ├── usb_msc_sd.cpp
│   ├── ftp_service.h
│   ├── ftp_service.cpp
│   ├── web_ui.h
│   ├── web_ui.cpp
│   ├── settings_store.h
│   ├── settings_store.cpp
│   ├── app_state.h
│   └── app_state.cpp
└── scripts/
    └── ftp_upload_wait.py
```

Recommended module roles:

- `sd_card`
  - SD SPI init and filesystem availability
- `usb_msc_sd`
  - MSC read/write export, enable/disable, refresh
- `ftp_service`
  - FTP server lifecycle and SD-backed file access
- `web_ui`
  - status page and control endpoints
- `settings_store`
  - persistent MSC/FTP enable flags
- `app_state`
  - runtime state and error tracking

---

## 14. Feature-by-feature implementation plan

## Phase 1: Project skeleton
Goal:
- PlatformIO + Arduino project builds and boots

## Phase 2: SD card module
Goal:
- reliable SD init with known-good SPI pins

## Phase 3: App state + settings persistence
Goal:
- central runtime state
- persist MSC/FTP enable flags

## Phase 4: Read/write MSC service
Goal:
- expose SD to host as Read/write USB MSC
- enable/disable/refresh support

## Phase 5: FTP service
Goal:
- enable/disable FTP using SD filesystem

## Phase 6: Web UI skeleton
Goal:
- status page + JSON/API status

## Phase 7: Web UI controls
Goal:
- enable/disable MSC
- refresh MSC
- enable/disable FTP

## Phase 8: Stability and polish
Goal:
- better error handling
- clear user messages
- cleaner host refresh workflow

---

## 15. Acceptance criteria

The project is minimally successful when all of the following work:

1. Device boots under PlatformIO + Arduino
2. SD card mounts successfully
3. MSC can be enabled from the web UI
4. MSC is Read/write on the host PC
5. FTP can be enabled from the web UI
6. FTP can upload a file to the SD card
7. After MSC refresh, the host PC can see newly uploaded files
8. MSC enabled state persists across reboot
9. FTP enabled state persists across reboot
10. By default, both MSC and FTP are disabled on a fresh safe boot

---

## 16. Developer workflow hints for implementation agents

## 16.1 Uploading firmware
Agents should assume PlatformIO commands similar to:

```bash
pio run
pio run -t upload
pio device monitor
```

## 16.2 Reset behavior
Agents should be aware of the previously observed behavior:

- some boards may not reliably reset the way PlatformIO expects
- manual reset may be needed after upload
- custom post-upload reset hooks may be needed depending on the host Mac and USB behavior

Agents should know if they need to reset the board they should use similar approach as upload_post_reset.py scripts

Implementation agents should preserve working upload/reset scripts if they already exist in the repository.

## 16.3 Serial logging
Serial logging is an important test/debug channel.

Agents should:
- reset the board first if important serial logging to be monitored from the setup()
- keep clear boot and service-state logs
- log MSC enable/disable/refresh
- log FTP enable/disable
- log SD init errors
- log web actions
- use the serial monitor during integration testing

## 16.4 FTP test workflow
Agents should use a Python FTP client script to test:

- login
- upload file
- close session

The existing or future test script should:
- connect to the ESP32 FTP server
- upload a known file
- quit cleanly

## 16.5 MSC host-side test workflow
The development PC should be used as an MSC host.

Agents should verify:
- host sees the MSC drive when enabled
- host can write to the drive
- host can read/copy files from it
- after FTP upload, host may need MSC refresh to see new files

## 16.6 Manual host behavior reminder
Agents must remember:
- host-side file visibility may lag until remount/re-enumeration
- this is expected in the shared one-partition design
- Refresh MSC is part of the intended workflow, not a bug workaround

---

## 17. Suggested test scenarios

## Test 1 — Boot defaults
- Boot device
- Confirm MSC disabled
- Confirm FTP disabled
- Confirm SD detected if present

## Test 2 — Enable MSC
- Enable MSC from web UI
- Confirm host sees drive
- Confirm drive is read-only

## Test 3 — Enable FTP and upload
- Enable FTP from web UI
- Use Python script to upload a file
- Confirm upload success in serial log

## Test 4 — Refresh MSC
- Trigger Refresh MSC from web UI
- Confirm host re-sees drive
- Confirm uploaded file appears on host

## Test 5 — Persistence
- Enable/disable MSC and FTP in different combinations
- Reboot
- Confirm saved states are restored safely

---

## 18. Non-goals for this phase

These are not required in the current simplified phase:

- two SD partitions
- direct FatFs multi-partition management
- ESP-IDF migration
- simultaneous host-coherent live updates without MSC refresh
- authenticated web UI
- HTTPS/TLS
- OTA update
- advanced sync/copy workflows

---

## 19. Summary

Final simplified architecture for this phase:

- **Arduino + PlatformIO**
- **one FAT SD partition**
- **MSC Read/write**
- **FTP read/write on same filesystem**
- **web UI for enable/disable MSC**
- **web UI for enable/disable FTP**
- **web UI for Refresh MSC**
- **persist MSC/FTP settings across reboot**
- **use serial log + Python FTP script + host MSC testing during development**
