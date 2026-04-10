/**
 * ESP32-S3 SD / Read-Only MSC / FTP / Web UI
 *
 * PR #1: Project skeleton
 * PR #2: SD card module (FSPI, Arduino SD library)
 */

#include <Arduino.h>
#include <SD.h>

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

    // PR #2: SD card init
    if (sd_card_init(&sd_cfg)) {
        Serial.println("[SD] Ready for next modules");
    } else {
        Serial.println("[SD] WARNING: No SD card - will continue without storage");
    }
}

void loop()
{
    delay(5000);
}