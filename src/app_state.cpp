#include "app_state.h"

#include <stdarg.h>
#include <stdio.h>

static app_state_t g_state = {};

void app_state_init(void)
{
    g_state.sd_ready = false;
    g_state.msc_enabled = false;
    g_state.ftp_enabled = false;
    g_state.wifi_connected = false;
    g_state.last_error[0] = '\0';

    Serial.println("[App] State initialized (all services disabled)");
}

const app_state_t *app_state_get(void)
{
    return &g_state;
}

void app_state_set_sd_ready(bool ready)
{
    g_state.sd_ready = ready;
    Serial.printf("[App] SD ready: %s\n", ready ? "yes" : "no");
    if (!ready) {
        app_state_set_error("SD card not available");
    }
}

void app_state_set_msc_enabled(bool enabled)
{
    g_state.msc_enabled = enabled;
    Serial.printf("[App] MSC enabled: %s\n", enabled ? "yes" : "no");
}

void app_state_set_ftp_enabled(bool enabled)
{
    g_state.ftp_enabled = enabled;
    Serial.printf("[App] FTP enabled: %s\n", enabled ? "yes" : "no");
}

void app_state_set_wifi_connected(bool connected)
{
    g_state.wifi_connected = connected;
    Serial.printf("[App] WiFi connected: %s\n", connected ? "yes" : "no");
    if (!connected) {
        app_state_set_error("WiFi not connected");
    }
}

void app_state_set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_state.last_error, APP_MAX_ERROR_LEN, fmt, args);
    va_end(args);

    Serial.printf("[App] Error: %s\n", g_state.last_error);
}

void app_state_clear_error(void)
{
    g_state.last_error[0] = '\0';
}