#include "settings_store.h"

#include <Preferences.h>

static Preferences g_prefs;
static settings_t g_settings = {};
static bool g_initialized = false;

static const char *PREFS_NAMESPACE = "app_settings";
static const char *KEY_MSC_ENABLED = "msc_en";
static const char *KEY_FTP_ENABLED = "ftp_en";
static const char *KEY_HTTP_MODE = "http_mode";

bool settings_init(void)
{
    g_prefs.begin(PREFS_NAMESPACE, true); // read-only

    // Check if keys exist (first boot detection)
    bool has_msc = g_prefs.isKey(KEY_MSC_ENABLED);
    bool has_ftp = g_prefs.isKey(KEY_FTP_ENABLED);
    bool has_http = g_prefs.isKey(KEY_HTTP_MODE);

    if (has_msc || has_ftp || has_http) {
        // Load saved settings
        g_settings.msc_enabled = g_prefs.getBool(KEY_MSC_ENABLED, true);  // Default: MSC enabled
        g_settings.ftp_enabled = g_prefs.getBool(KEY_FTP_ENABLED, false);
        g_settings.http_mode = g_prefs.getBool(KEY_HTTP_MODE, false);   // Legacy
        g_prefs.end();
        g_initialized = true;
        Serial.printf("[Settings] Loaded: MSC=%s\n",
                      g_settings.msc_enabled ? "on" : "off");
        return true;
    }

    // First boot or no saved settings — defaults
    g_settings.msc_enabled = true;   // Default: MSC enabled on boot
    g_settings.ftp_enabled = false;
    g_settings.http_mode = false;
    g_prefs.end();
    g_initialized = true;

    Serial.println("[Settings] No saved settings, using defaults (MSC enabled)");
    return false;
}

const settings_t *settings_get(void)
{
    return &g_settings;
}

void settings_set_msc_mode(bool enabled)
{
    g_settings.msc_enabled = enabled;
    g_settings.http_mode = !enabled;  // Legacy compatibility

    if (g_initialized) {
        g_prefs.begin(PREFS_NAMESPACE, false); // read-write
        g_prefs.putBool(KEY_MSC_ENABLED, enabled);
        g_prefs.putBool(KEY_HTTP_MODE, !enabled);
        g_prefs.end();
    }

    Serial.printf("[Settings] MSC mode = %s (saved)\n", enabled ? "on" : "off");
}

void settings_set_msc_enabled(bool enabled)
{
    settings_set_msc_mode(enabled);
}

void settings_set_ftp_enabled(bool enabled)
{
    g_settings.ftp_enabled = enabled;

    if (g_initialized) {
        g_prefs.begin(PREFS_NAMESPACE, false); // read-write
        g_prefs.putBool(KEY_FTP_ENABLED, enabled);
        g_prefs.end();
    }

    Serial.printf("[Settings] FTP enabled = %s (saved)\n", enabled ? "on" : "off");
}

void settings_set_http_mode(bool enabled)
{
    // Legacy: http_mode=true means MSC is OFF
    settings_set_msc_mode(!enabled);
}

void settings_reset(void)
{
    g_settings.msc_enabled = true;   // Reset to MSC enabled
    g_settings.ftp_enabled = false;
    g_settings.http_mode = false;

    if (g_initialized) {
        g_prefs.begin(PREFS_NAMESPACE, false);
        g_prefs.clear();
        g_prefs.end();
    }

    Serial.println("[Settings] Reset to defaults (MSC enabled)");
}
