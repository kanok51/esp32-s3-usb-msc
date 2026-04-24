#pragma once

#include <Arduino.h>

// Settings that persist across reboot via NVS
struct settings_t {
    bool msc_enabled;      // MSC active on boot (default: true)
    bool ftp_enabled;      // FTP on/off (deprecated)
    bool http_mode;        // Legacy, now mapped to !msc_enabled
};

// Initialize settings store and load from NVS
// Returns true if saved settings were found and loaded
bool settings_init(void);

// Get current settings (read-only)
const settings_t *settings_get(void);

// Update and persist MSC mode setting
// true = MSC enabled on boot (SD as USB)
// false = MSC disabled on boot (HTTP can access SD)
void settings_set_msc_mode(bool enabled);

// Legacy compatibility
void settings_set_msc_enabled(bool enabled);
void settings_set_ftp_enabled(bool enabled);
void settings_set_http_mode(bool enabled);

// Reset all settings to defaults and persist
void settings_reset(void);