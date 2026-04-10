/**
 * ESP32-S3 SD / Read-Only MSC / FTP / Web UI
 *
 * PR #1: Project skeleton
 * - PlatformIO + Arduino project builds and boots
 * - Boot banner on serial
 * - Minimal main loop
 */

#include <Arduino.h>
#include "config.h"

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  ESP32-S3 SD / MSC / FTP / Web UI");
    Serial.println("  PR #1: Project skeleton");
    Serial.println("========================================");
    Serial.println();

    Serial.printf("SD pins: SCK=%d MISO=%d MOSI=%d CS=%d\n",
                  APP_SD_SCK, APP_SD_MISO, APP_SD_MOSI, APP_SD_CS);
    Serial.printf("WiFi SSID: %s\n", APP_WIFI_SSID);
    Serial.println();
    Serial.println("Setup complete. Waiting for next PR modules.");
}

void loop()
{
    delay(5000);
}