/**
 * ESP32-S3 SD / Read-Only MSC / FTP / Web UI
 *
 * PR #1: Project skeleton
 * PR #2: SD card module
 * PR #3: App state model
 * PR #4: Settings persistence (NVS)
 * PR #5: Read-only MSC module
 */

#include <Arduino.h>
#include <SD.h>

#include "app_state.h"
#include "sd_card.h"
#include "settings_store.h"
#include "usb_msc_sd.h"
#include "config.h"

static sd_card_config_t sd_cfg = {
    .sck = APP_SD_SCK,
    .miso = APP_SD_MISO,
    .mosi = APP_SD_MOSI,
    .cs = APP_SD_CS,
    .freq_hz = APP_SD_FREQ_HZ,
    .mount_point = APP_SD_MOUNT_POINT
};

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  ESP32-S3 SD / MSC / FTP / Web UI");
    Serial.println("========================================");
    Serial.println();

    // PR #3: Initialize app state (all services disabled)
    app_state_init();

    // PR #4: Load persisted settings (falls back to all disabled)
    bool has_settings = settings_init();
    if (!has_settings) {
        Serial.println("[Main] Fresh boot — all services disabled by default");
    } else {
        const settings_t *s = settings_get();
        Serial.printf("[Main] Restored settings: MSC=%s FTP=%s\n",
                      s->msc_enabled ? "on" : "off",
                      s->ftp_enabled ? "on" : "off");
        // Apply saved states to app state
        app_state_set_msc_enabled(s->msc_enabled);
        app_state_set_ftp_enabled(s->ftp_enabled);
    }

    // PR #2: SD card init
    bool sd_ok = sd_card_init(&sd_cfg);
    app_state_set_sd_ready(sd_ok);

    if (sd_ok) {
        Serial.println("[Main] SD card ready");

        // PR #5: Enable MSC for testing (will be controlled by settings in final version)
        Serial.println("[Main] Enabling MSC for PR #5 validation...");
        if (usb_msc_sd_begin()) {
            Serial.println("[Main] MSC enabled successfully");
            app_state_set_msc_enabled(true);
        } else {
            Serial.println("[Main] MSC enable failed");
        }
    } else {
        Serial.println("[Main] No SD card — services will be limited");
    }
}

void loop()
{
    delay(5000);
}
