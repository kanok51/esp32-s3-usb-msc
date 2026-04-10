#pragma once

#include <Arduino.h>

// Settings that persist across reboot via NVS
struct settings_t {
    bool msc_enabled;
    bool ftp_enabled;
};

// Initialize settings store and load from NVS
// Returns true if saved settings were found and loaded
bool settings_init(void);

// Get current settings (read-only)
const settings_t *settings_get(void);

// Update and persist a single setting
void settings_set_msc_enabled(bool enabled);
void settings_set_ftp_enabled(bool enabled);

// Reset all settings to defaults (both disabled) and persist
void settings_reset(void);