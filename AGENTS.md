# AGENTS.md

## Source of truth

Follow these files in order:

1. `arduino_platformio_sd_msc_ftp_requirements.md`
2. `arduino_platformio_sd_msc_ftp_pr_plan.md`

If there is a conflict, follow the requirements file first.

## Framework and tooling

- Use **Arduino framework**
- Use **PlatformIO**
- Do not migrate this project to ESP-IDF in the current phase
- Keep `platformio.ini` as the primary build entry point

## Current architecture

- One SD card FAT filesystem
- USB MSC must be **read-only**
- FTP reads/writes the same SD filesystem
- Web UI must support:
  - enable MSC
  - disable MSC
  - refresh MSC
  - enable FTP
  - disable FTP
- MSC and FTP must both be **disabled by default**
- MSC and FTP enable states must persist across reboot

## Important behavior

- The host PC may not immediately see FTP-uploaded files while MSC is already mounted
- This is expected in the current design
- **Refresh MSC** is an intended feature and workflow
- Do not redesign this into a multi-partition or ESP-IDF solution unless explicitly asked

## Validation requirements

Before considering work complete, verify as applicable:

- PlatformIO build succeeds
- Upload works
- Serial logs are clear for boot and service state changes
- FTP upload works using the Python test script
- MSC is visible from the development PC when enabled
- MSC is read-only from the host side
- Newly uploaded FTP files appear after MSC refresh

## Board workflow notes

- Some hosts may require manual reset after upload
- Preserve any known working upload/reset helper scripts
- Do not remove post-upload reset hooks unless replaced by a verified working alternative

## Implementation style

- Prefer small, reviewable PRs
- Do not combine unrelated refactors with feature work
- Preserve already working SD / MSC / FTP behavior where possible
- Add clear serial logs for SD, MSC, FTP, and web actions

## Documentation

If workflow or architecture changes, update:
- `arduino_platformio_sd_msc_ftp_requirements.md`
- `arduino_platformio_sd_msc_ftp_pr_plan.md`
- relevant test instructions