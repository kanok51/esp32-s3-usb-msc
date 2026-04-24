#include "ftp_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>

// SimpleFTPServer library - MUST set storage type BEFORE including header
#define DEFAULT_FTP_SERVER_NETWORK_TYPE_ESP32 NETWORK_ESP32
#define DEFAULT_STORAGE_TYPE_ESP32 STORAGE_SD  // Use Arduino SD library
#include <FtpServer.h>

#include "config.h"
#include "sd_card.h"
#include "usb_msc_sd.h"

static FtpServer g_ftp;
static bool g_ftp_started = false;
static bool g_ftp_client_connected = false;

// Mode switch flags (set by callback, handled by poll)
static volatile bool g_request_switch_to_ftp_mode = false;
static volatile bool g_request_switch_to_msc_mode = false;
static bool g_msc_enabled = false;
static bool g_ignore_ftp_disconnect = false;

// Remount SD for filesystem access (after MSC was using it)
static bool remount_sd_for_fs(void)
{
    // FIX: Ensure MSC is completely disabled before attempting SD remount
    // This prevents SD card contention between MSC and filesystem operations
    if (usb_msc_sd_is_active()) {
        Serial.println("[FTP] Stopping MSC before SD remount...");
        usb_msc_sd_end();
        delay(300);  // Allow USB MSC cleanup
    }
    
    SD.end();
    delay(150);

    sd_card_config_t sd_cfg = {
        .sck = APP_SD_SCK,
        .miso = APP_SD_MISO,
        .mosi = APP_SD_MOSI,
        .cs = APP_SD_CS,
        .freq_hz = APP_SD_FREQ_HZ,
        .mount_point = APP_SD_MOUNT_POINT
    };

    if (!sd_card_init(&sd_cfg)) {
        Serial.println("[FTP] ERROR: SD remount failed");
        return false;
    }

    delay(100);
    return true;
}

static void ftp_stop_server(void)
{
    if (!g_ftp_started) {
        return;
    }

    g_ignore_ftp_disconnect = true;
    g_ftp.end();
    g_ftp_started = false;
    g_ftp_client_connected = false;
    delay(50);
    g_ignore_ftp_disconnect = false;

    Serial.println("[FTP] Server stopped");
}

static void ftp_start_server(const char *user, const char *pass)
{
    if (g_ftp_started) {
        return;
    }

    g_ftp.begin(user, pass, "ESP32-S3 FTP Server");
    g_ftp_started = true;
    
    Serial.println("[FTP] Server started");
}

// FTP transfer callback for debugging
static void ftp_transfer_callback(FtpTransferOperation op, const char *name, uint32_t transferred_size) {
    (void)transferred_size;
    switch (op) {
        case FTP_UPLOAD_START:
            Serial.printf("[FTP] Upload start: %s\n", name ? name : "unknown");
            break;
        case FTP_DOWNLOAD_START:
            Serial.printf("[FTP] Download start: %s\n", name ? name : "unknown");
            break;
        case FTP_TRANSFER_STOP:
            Serial.printf("[FTP] Transfer stop: %s\n", name ? name : "unknown");
            break;
        case FTP_TRANSFER_ERROR:
            Serial.printf("[FTP] Transfer error: %s\n", name ? name : "unknown");
            break;
        default:
            break;
    }
}

// FTP callback - sets request flags only
static void ftp_callback(FtpOperation op, uint32_t free_space, uint32_t total_space)
{
    (void)total_space;

    switch (op) {
        case FTP_CONNECT:
            Serial.println("[FTP] Client connected - requesting FTP mode");
            g_ftp_client_connected = true;
            // Only request mode switch if MSC is actually active
            if (usb_msc_sd_is_active()) {
                g_request_switch_to_ftp_mode = true;
            }
            break;
        case FTP_DISCONNECT:
            if (g_ignore_ftp_disconnect) {
                Serial.println("[FTP] Ignoring disconnect during mode switch");
                break;
            }
            Serial.println("[FTP] Client disconnected - requesting MSC mode");
            g_ftp_client_connected = false;
            if (!g_msc_enabled) {
                g_request_switch_to_msc_mode = true;
            }
            break;
        case FTP_FREE_SPACE_CHANGE:
            Serial.printf("[FTP] Free space: %u MB / %u MB\n", 
                          free_space / (1024*1024), total_space / (1024*1024));
            break;
    }
}

bool ftp_service_init(void)
{
    Serial.println("[FTP] Service initialized");
    return true;
}

bool ftp_service_begin(const ftp_service_config_t *config)
{
    if (g_ftp_started) {
        Serial.println("[FTP] Already started");
        return true;
    }

    if (!WiFi.isConnected()) {
        Serial.println("[FTP] ERROR: WiFi not connected");
        return false;
    }

    // FIX: Guard against MSC active state - prevent filesystem contention
    // Must stop MSC before SD can be used for filesystem operations
    if (usb_msc_sd_is_active()) {
        Serial.println("[FTP] MSC is active - stopping MSC before FTP can start");
        usb_msc_sd_end();
        delay(300);  // Allow MSC cleanup
        
        // REMOUNT SD for filesystem access (only needed if MSC was active)
        Serial.println("[FTP] Remounting SD for FTP access...");
        if (!remount_sd_for_fs()) {
            Serial.println("[FTP] ERROR: SD remount failed");
            return false;
        }
    } else {
        Serial.println("[FTP] MSC not active - using existing SD mount");
    }

    const char *user = (config && config->ftp_user) ? config->ftp_user : "esp32";
    const char *pass = (config && config->ftp_password) ? config->ftp_password : "esp32pass";

    // Set callback
    g_ftp.setCallback(ftp_callback);
    g_ftp.setTransferCallback(ftp_transfer_callback);
    
    // Start FTP server
    ftp_start_server(user, pass);
    
    Serial.printf("[FTP] Credentials: %s / %s\n", user, pass);
    Serial.printf("[FTP] Root: /sd\n");
    
    return true;
}

void ftp_service_end(void)
{
    ftp_stop_server();
    Serial.println("[FTP] Service ended");
}

void ftp_service_poll(void)
{
    if (!WiFi.isConnected()) {
        return;
    }

    // Handle switch to FTP mode (MSC -> FTP)
    if (g_request_switch_to_ftp_mode) {
        g_request_switch_to_ftp_mode = false;

        Serial.println("[FTP] Switching from MSC mode to FTP mode...");

        ftp_stop_server();
        delay(150);

        if (g_msc_enabled) {
            usb_msc_sd_end();
            g_msc_enabled = false;
            delay(300);
        }

        if (!remount_sd_for_fs()) {
            Serial.println("[FTP] ERROR: Failed to remount SD for FTP mode");
            // Try to recover by restarting FTP anyway
            ftp_start_server("esp32", "esp32pass");
            return;
        }

        Serial.println("[FTP] SD remounted for FTP mode");
        ftp_start_server("esp32", "esp32pass");
        return;
    }

    // Handle switch to MSC mode (FTP -> MSC)
    if (g_request_switch_to_msc_mode) {
        g_request_switch_to_msc_mode = false;

        Serial.println("[FTP] Switching from FTP mode to MSC mode...");

        ftp_stop_server();
        delay(150);

        SD.end();
        delay(300);

        if (usb_msc_sd_begin()) {
            g_msc_enabled = true;
            Serial.println("[FTP] MSC re-enabled");
        } else {
            g_msc_enabled = false;
            Serial.println("[FTP] ERROR: MSC restart failed");
        }

        // Restart FTP server (it will handle new connections in MSC+FTP mode)
        ftp_start_server("esp32", "esp32pass");
        return;
    }

    // Normal FTP handling
    if (g_ftp_started) {
        g_ftp.handleFTP();
    }
}

bool ftp_service_is_active(void)
{
    return g_ftp_started;
}

bool ftp_service_is_client_connected(void)
{
    return g_ftp_client_connected;
}

IPAddress ftp_service_ip(void)
{
    return WiFi.localIP();
}

// Call this from main to initialize MSC state
void ftp_service_set_msc_enabled(bool enabled)
{
    g_msc_enabled = enabled;
}
