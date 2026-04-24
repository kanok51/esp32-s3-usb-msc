/**
 * ESP32-S3 SD Card - HTTP Always On + MSC Toggle
 * 
 * Architecture:
 * - HTTP server ALWAYS runs (for control/status/mode switching)
 * - MSC can be enabled/disabled independently
 * - When MSC ON: SD card as USB drive, HTTP shows status (no SD access)
 * - When MSC OFF: HTTP can access SD card for uploads
 * - Mutual exclusion at SD access level, not HTTP server level
 */

#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <USB.h>
#include <USBMSC.h>

#include "sd_card.h"
#include "usb_msc_sd.h"
#include "http_upload_service.h"
#include "settings_store.h"
#include "config.h"

static sd_card_config_t sd_cfg = {
    .sck = APP_SD_SCK,
    .miso = APP_SD_MISO,
    .mosi = APP_SD_MOSI,
    .cs = APP_SD_CS,
    .freq_hz = APP_SD_FREQ_HZ,
    .mount_point = APP_SD_MOUNT_POINT
};

static http_upload_config_t http_cfg = {
    .wifi_ssid = APP_WIFI_SSID,
    .wifi_password = APP_WIFI_PASSWORD,
    .server_port = 80
};

// State
static bool g_wifi_connected = false;
static bool g_msc_active = false;

void print_banner(void)
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("  ESP32-S3 SD Card Interface");
    Serial.println("  HTTP Always On + MSC Toggle");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Architecture:");
    Serial.println("  - HTTP server: ALWAYS ON");
    Serial.println("  - MSC mode: Toggle SD as USB drive");
    Serial.println("  - SD access: MSC OR HTTP (mutual exclusion)");
    Serial.println();
}

void print_status(void)
{
    Serial.println();
    Serial.println("========================================");
    Serial.printf("  WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  HTTP Server: %s\n", http_upload_is_running() ? "RUNNING" : "OFF");
    Serial.printf("  MSC Mode: %s\n", g_msc_active ? "ACTIVE (SD as USB)" : "OFF (HTTP can access SD)");
    Serial.printf("  SD Access: %s\n", g_msc_active ? "USB MSC only" : "HTTP only");
    Serial.println("========================================");
    Serial.println();
    
    if (g_msc_active) {
        Serial.println("Current State: MSC MODE");
        Serial.println("  - SD card accessible as USB drive");
        Serial.println("  - HTTP server running (status/control)");
        Serial.println("  - Upload via HTTP: BLOCKED (use USB)");
        Serial.println();
        Serial.printf("  Web Interface: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.println("  (For mode switching and status)");
    } else {
        Serial.println("Current State: HTTP MODE");
        Serial.println("  - SD card accessible via HTTP uploads");
        Serial.println("  - HTTP server running (full access)");
        Serial.println("  - USB MSC: DISABLED");
        Serial.println();
        Serial.printf("  Upload URL: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    
    Serial.println();
    Serial.println("Serial Commands:");
    Serial.println("  'm' - Enable MSC mode (SD as USB drive)");
    Serial.println("  'h' - Disable MSC mode (HTTP access to SD)");
    Serial.println("  'r' - Reset settings");
    Serial.println();
    Serial.println("HTTP Endpoints:");
    Serial.printf("  http://%s/       - Status and upload interface\n", WiFi.localIP().toString().c_str());
    Serial.printf("  http://%s/msc/on  - Enable MSC mode\n", WiFi.localIP().toString().c_str());
    Serial.printf("  http://%s/msc/off - Disable MSC mode\n", WiFi.localIP().toString().c_str());
    Serial.println();
}

bool wifi_connect(void)
{
    if (WiFi.status() == WL_CONNECTED) {
        g_wifi_connected = true;
        return true;
    }
    
    Serial.println("[WiFi] Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASSWORD);
    
    const uint32_t timeout = 30000;
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
    g_wifi_connected = true;
    return true;
}

void wifi_keepalive(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        if (g_wifi_connected) {
            Serial.println("[WiFi] Connection lost, reconnecting...");
            g_wifi_connected = false;
        }
        wifi_connect();
    }
}

void enable_msc_mode(void)
{
    if (g_msc_active) {
        Serial.println("[MSC] Already active");
        return;
    }
    
    Serial.println();
    Serial.println("[MSC] Enabling MSC mode...");
    Serial.println("[MSC] HTTP server continues running");
    Serial.println("[MSC] SD card will be accessible via USB");
    Serial.println();
    
    // Tell HTTP service that MSC is taking over SD access
    http_upload_set_msc_active(true);
    
    // Small delay to ensure HTTP releases SD
    delay(100);
    
    // Re-initialize SD for MSC
    SD.end();
    delay(100);
    
    if (!sd_card_init(&sd_cfg)) {
        Serial.println("[MSC] ERROR: SD re-init failed");
        http_upload_set_msc_active(false);
        return;
    }
    
    // Start MSC
    if (usb_msc_sd_begin()) {
        g_msc_active = true;
        settings_set_msc_mode(true);
        Serial.println("[MSC] Mode enabled - SD as USB drive");
        print_status();
    } else {
        Serial.println("[MSC] ERROR: Failed to start MSC");
        http_upload_set_msc_active(false);
    }
}

void disable_msc_mode(void)
{
    if (!g_msc_active) {
        Serial.println("[MSC] Already disabled");
        return;
    }
    
    Serial.println();
    Serial.println("[MSC] Disabling MSC mode...");
    Serial.println("[MSC] SD access switching to HTTP");
    Serial.println();
    
    // Stop MSC
    usb_msc_sd_end();
    g_msc_active = false;
    
    // Re-initialize SD for HTTP access
    SD.end();
    delay(100);
    
    if (!sd_card_init(&sd_cfg)) {
        Serial.println("[MSC] WARNING: SD re-init failed");
    } else {
        Serial.println("[MSC] SD card ready for HTTP access");
    }
    
    // Tell HTTP service it can now access SD
    http_upload_set_msc_active(false);
    
    settings_set_msc_mode(false);
    print_status();
}

void handle_serial_input(void)
{
    if (!Serial.available()) {
        return;
    }
    
    char c = Serial.read();
    
    // Echo the command
    Serial.println();
    Serial.printf("[Serial] Command received: '%c'\n", c);
    
    switch (c) {
        case 'm':
        case 'M':
            enable_msc_mode();
            break;
            
        case 'h':
        case 'H':
            disable_msc_mode();
            break;
            
        case 'r':
        case 'R':
            Serial.println("[Serial] Resetting settings to defaults...");
            settings_reset();
            Serial.println("[Serial] MSC will be enabled on next boot (default)");
            break;
            
        case 's':
        case 'S':
            print_status();
            break;
            
        case '\n':
        case '\r':
            // ignore
            break;
            
        default:
            Serial.println("  Available commands:");
            Serial.println("    'm' - Enable MSC mode (SD as USB drive)");
            Serial.println("    'h' - Disable MSC mode (HTTP access to SD)");
            Serial.println("    's' - Show status");
            Serial.println("    'r' - Reset settings");
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    print_banner();

    // Initialize settings
    bool has_settings = settings_init();
    const settings_t *s = settings_get();

    // Initialize SD card FIRST
    Serial.println("[Main] Initializing SD card...");
    bool sd_ok = sd_card_init(&sd_cfg);
    
    if (!sd_ok) {
        Serial.println("[Main] ERROR: SD card init failed");
        Serial.println("[Main] Halting...");
        while (1) {
            delay(1000);
        }
    }

    Serial.println("[Main] SD card initialized");
    Serial.printf("[Main] Size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
    
    // Start WiFi
    Serial.println("[Main] Starting WiFi...");
    if (wifi_connect()) {
        Serial.println("[Main] WiFi ready");
    } else {
        Serial.println("[Main] WARNING: WiFi not connected, will retry");
    }
    
    // Start HTTP server (ALWAYS ON)
    Serial.println("[Main] Starting HTTP server (always on)...");
    http_upload_init();
    if (http_upload_begin(&http_cfg)) {
        Serial.println("[Main] HTTP server started");
    } else {
        Serial.println("[Main] ERROR: HTTP server failed to start");
    }
    
    // Check saved mode preference
    bool start_with_msc = true; // Default to MSC enabled
    if (has_settings) {
        start_with_msc = s->msc_enabled;
    }
    
    if (start_with_msc) {
        Serial.println("[Main] Default: MSC mode enabled");
        enable_msc_mode();
    } else {
        Serial.println("[Main] Default: HTTP mode (MSC disabled)");
        // HTTP already running, SD already accessible via HTTP
        http_upload_set_msc_active(false);
        g_msc_active = false;
        print_status();
    }
    
    Serial.println("========================================");
    Serial.println("Setup Complete");
    Serial.println("========================================");
    Serial.println();
}

void loop()
{
    // WiFi keepalive
    wifi_keepalive();
    
    // HTTP server always runs
    http_upload_poll();
    
    // Check for serial commands
    handle_serial_input();
    
    // Check for HTTP-based mode switch requests
    if (http_upload_should_enable_msc()) {
        enable_msc_mode();
    } else if (http_upload_should_disable_msc()) {
        disable_msc_mode();
    }
    
    delay(10);
}
