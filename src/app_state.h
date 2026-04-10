#pragma once

#include <Arduino.h>

// Maximum length for last error message
#define APP_MAX_ERROR_LEN 128

struct app_state_t {
    bool sd_ready;
    bool msc_enabled;
    bool ftp_enabled;
    bool wifi_connected;
    char last_error[APP_MAX_ERROR_LEN];
};

// Initialize app state to safe defaults
void app_state_init(void);

// Get singleton app state (read-only access)
const app_state_t *app_state_get(void);

// Setters (also update last_error on failure)
void app_state_set_sd_ready(bool ready);
void app_state_set_msc_enabled(bool enabled);
void app_state_set_ftp_enabled(bool enabled);
void app_state_set_wifi_connected(bool connected);

// Set last error message
void app_state_set_error(const char *fmt, ...);

// Clear last error
void app_state_clear_error(void);