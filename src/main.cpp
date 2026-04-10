/**
 * ESP32-S3 SD / Read-Only MSC / FTP / Web UI
 *
 * PR #1: Project skeleton
 * PR #2: SD card module
 * PR #3: App state model
 */

#include <Arduino.h>
#include <SD.h>

#include "app_state.h"
#include "sd_card.h"
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

    // PR #2: SD card init
    bool sd_ok = sd_card_init(&sd_cfg);
    app_state_set_sd_ready(sd_ok);

    if (sd_ok) {
        Serial.println("[Main] SD card ready");
    } else {
        Serial.println("[Main] No SD card - services will be limited");
    }
}

void loop()
{
    delay(5000);
}