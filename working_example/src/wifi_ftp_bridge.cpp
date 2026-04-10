#include "wifi_ftp_bridge.h"

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>

#define DEFAULT_FTP_SERVER_NETWORK_TYPE_ESP32 NETWORK_ESP32
#define DEFAULT_STORAGE_TYPE_ESP32 STORAGE_SD
#include <FtpServer.h>

#include "usb_msc_sd.h"
#include "sd_card.h"
#include "config.h"

static FtpServer g_ftp;

static bool g_wifi_ready = false;
static bool g_ftp_connected = false;
static bool g_ftp_server_started = false;
static bool g_msc_enabled = false;

static bool g_request_switch_to_ftp_mode = false;
static bool g_request_switch_to_msc_mode = false;

// Important guard: ignore FTP disconnect callback while we are
// intentionally stopping the FTP server during a mode transition.
static bool g_ignore_ftp_disconnect = false;

static wifi_ftp_bridge_config_t g_cfg = {};

static bool wifi_connect_sta(const char *ssid, const char *password, uint32_t timeout_ms)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print('.');
        if ((millis() - start) > timeout_ms) {
            Serial.println();
            Serial.println("WiFi connect timeout");
            return false;
        }
    }

    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

static bool remount_sd_for_fs(void)
{
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
        return false;
    }

    delay(100);
    return true;
}

static void ftp_status_callback(FtpOperation op, uint32_t free_space, uint32_t total_space)
{
    (void)free_space;
    (void)total_space;

    switch (op) {
        case FTP_CONNECT:
            g_ftp_connected = true;

            if (g_msc_enabled) {
                Serial.println("FTP client connected while MSC active -> scheduling FTP mode switch");
                g_request_switch_to_ftp_mode = true;
            }
            break;

        case FTP_DISCONNECT:
            g_ftp_connected = false;

            if (g_ignore_ftp_disconnect) {
                Serial.println("Ignoring FTP disconnect during intentional server stop");
                break;
            }

            if (!g_msc_enabled) {
                Serial.println("FTP client disconnected while in FTP mode -> scheduling MSC mode switch");
                g_request_switch_to_msc_mode = true;
            }
            break;

        default:
            break;
    }
}

static void ftp_transfer_callback(FtpTransferOperation op, const char *name, uint32_t transferred_size)
{
    (void)name;
    (void)transferred_size;

    switch (op) {
        case FTP_UPLOAD_START:
            Serial.println("FTP upload start");
            break;
        case FTP_DOWNLOAD_START:
            Serial.println("FTP download start");
            break;
        case FTP_TRANSFER_STOP:
            Serial.println("FTP transfer stop");
            break;
        case FTP_TRANSFER_ERROR:
            Serial.println("FTP transfer error");
            break;
        default:
            break;
    }
}

static void ftp_start_server(void)
{
    if (g_ftp_server_started) {
        return;
    }

    g_ftp.setCallback(ftp_status_callback);
    g_ftp.setTransferCallback(ftp_transfer_callback);
    g_ftp.begin(g_cfg.ftp_user, g_cfg.ftp_password);

    g_ftp_server_started = true;
    Serial.println("FTP server started");
}

static void ftp_stop_server(void)
{
    if (!g_ftp_server_started) {
        return;
    }

    g_ignore_ftp_disconnect = true;

    g_ftp.end();
    g_ftp_server_started = false;
    g_ftp_connected = false;

    delay(50);
    g_ignore_ftp_disconnect = false;

    Serial.println("FTP server stopped");
}

bool wifi_ftp_bridge_begin(const wifi_ftp_bridge_config_t *config)
{
    if (config == nullptr ||
        config->wifi_ssid == nullptr ||
        config->wifi_password == nullptr ||
        config->ftp_user == nullptr ||
        config->ftp_password == nullptr) {
        return false;
    }

    g_cfg = *config;

    if (!wifi_connect_sta(g_cfg.wifi_ssid, g_cfg.wifi_password, 15000)) {
        g_wifi_ready = false;
        return false;
    }

    g_wifi_ready = true;

    if (usb_msc_sd_begin()) {
        g_msc_enabled = true;
        Serial.println("MSC enabled");
    } else {
        g_msc_enabled = false;
        Serial.println("MSC start skipped/failed");
    }

    ftp_start_server();
    return true;
}

void wifi_ftp_bridge_poll(void)
{
    if (!g_wifi_ready) {
        return;
    }

    if (g_request_switch_to_ftp_mode) {
        g_request_switch_to_ftp_mode = false;

        Serial.println("Switching from MSC mode to FTP mode...");

        ftp_stop_server();
        delay(150);

        if (g_msc_enabled) {
            usb_msc_sd_end();
            g_msc_enabled = false;
            delay(300);
        }

        if (!remount_sd_for_fs()) {
            Serial.println("Failed to remount SD for FTP mode");
            ftp_start_server();
            return;
        }

        Serial.println("SD remounted for FTP mode");
        ftp_start_server();
        return;
    }

    if (g_request_switch_to_msc_mode) {
        g_request_switch_to_msc_mode = false;

        Serial.println("Switching from FTP mode to MSC mode...");

        ftp_stop_server();
        delay(150);

        SD.end();
        delay(300);

        if (usb_msc_sd_begin()) {
            g_msc_enabled = true;
            Serial.println("MSC re-enabled");
        } else {
            g_msc_enabled = false;
            Serial.println("MSC restart failed");
        }

        ftp_start_server();
        return;
    }

    if (g_ftp_server_started) {
        g_ftp.handleFTP();
    }
}

bool wifi_ftp_bridge_is_wifi_ready(void)
{
    return g_wifi_ready;
}

bool wifi_ftp_bridge_is_ftp_connected(void)
{
    return g_ftp_connected;
}

IPAddress wifi_ftp_bridge_ip(void)
{
    return WiFi.localIP();
}