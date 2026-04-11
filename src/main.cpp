/**
 * ESP32-S3 SD / Read/Write MSC / FTP / Web UI
 *
 * PR #1: Project skeleton
 * PR #2: SD card module
 * PR #3: App state model
 * PR #4: Settings persistence (NVS)
 * PR #5: Read/Write MSC module
 * PR #6: FTP service module
 */

#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>

#include "app_state.h"
#include "sd_card.h"
#include "settings_store.h"
#include "usb_msc_sd.h"
#include "ftp_service.h"
#include "config.h"

static sd_card_config_t sd_cfg = {
    .sck = APP_SD_SCK,
    .miso = APP_SD_MISO,
    .mosi = APP_SD_MOSI,
    .cs = APP_SD_CS,
    .freq_hz = APP_SD_FREQ_HZ,
    .mount_point = APP_SD_MOUNT_POINT
};

static bool wifi_connect(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASSWORD);
    
    Serial.print("[WiFi] Connecting");
    const uint32_t timeout = 30000; // 30 seconds
    const uint32_t start = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
        if (millis() - start > timeout) {
            Serial.println("\n[WiFi] Connection timeout");
            return false;
        }
    }
    
    Serial.println();
    Serial.printf("[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

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
        // For testing: enable MSC on first boot
        settings_set_msc_enabled(true);
        app_state_set_msc_enabled(true);
        Serial.println("[Main] MSC auto-enabled for testing");
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

        // PR #5: Enable MSC if persisted setting says so
        if (usb_msc_sd_begin()) {
                Serial.println("[Main] MSC auto-enabled from saved settings");
                ftp_service_set_msc_enabled(true);  // Tell FTP service MSC is active
            } else {
                Serial.println("[Main] MSC auto-enable failed");
            }
        
        // PR #6: Initialize FTP service
        ftp_service_init();
        
        // Connect WiFi and start FTP
        if (wifi_connect()) {
            app_state_set_wifi_connected(true);
            
            // For testing: auto-enable FTP
            ftp_service_config_t ftp_cfg = {
                .ftp_user = "esp32",
                .ftp_password = "esp32pass",
                .ftp_port = 21
            };
            if (ftp_service_begin(&ftp_cfg)) {
                Serial.printf("[Main] FTP ready at ftp://%s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("[Main] FTP start failed");
            }
        } else {
            Serial.println("[Main] WiFi connection failed - FTP not started");
        }
    } else {
        Serial.println("[Main] No SD card — services will be limited");
    }
}

void loop()
{
    // PR #6: Poll FTP server
    ftp_service_poll();
    
    delay(10);
}
