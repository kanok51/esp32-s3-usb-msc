/**
 * PR #2: FTP Server + USB MSC with Internal Flash FAT
 * 
 * Features:
 * - FTP server on port 21 (passive mode) via SimpleFTPServer
 * - USB Mass Storage (MSC) - appears as FAT16 drive on USB
 * - Files stored on internal flash via FFat (2MB FAT partition)
 * - WiFi Station mode
 * 
 * WiFi: pukushome_fritz / amarnetamikhabo
 */

#include <Arduino.h>
#include <WiFi.h>
#include <FFat.h>
#include <SimpleFTPServer.h>
#include "USB.h"
#include "USBMSC.h"

extern "C" {
#include "esp_partition.h"
}

#ifndef WIFI_SSID
#define WIFI_SSID "pukushome_fritz"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "amarnetamikhabo"
#endif

#define FTP_PORT 21
#define BLOCK_SIZE 512

// --- USB MSC ---
USBMSC MSC;
static const esp_partition_t *g_part = nullptr;

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    if (!g_part) return -1;
    uint32_t addr = lba * BLOCK_SIZE + offset;
    if (addr + bufsize > g_part->size) return -1;
    return (esp_partition_read(g_part, addr, buffer, bufsize) == ESP_OK) ? (int32_t)bufsize : -1;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    if (!g_part) return -1;
    uint32_t addr = lba * BLOCK_SIZE + offset;
    if (addr + bufsize > g_part->size) return -1;

    // Flash requires erase-before-write; handle per 4KB sectors
    uint32_t written = 0;
    while (written < bufsize) {
        uint32_t sector_base = (addr + written) & ~0xFFF;  // 4KB aligned
        uint32_t sector_end = sector_base + 0x1000;
        uint32_t chunk = min(bufsize - written, sector_end - (addr + written));

        // Read-modify-write for partial sector writes
        uint8_t *tmp = (uint8_t *)malloc(0x1000);
        if (!tmp) return -1;

        if (esp_partition_read(g_part, sector_base, tmp, 0x1000) != ESP_OK) {
            free(tmp); return -1;
        }
        memcpy(tmp + ((addr + written) - sector_base), buffer + written, chunk);
        if (esp_partition_erase_range(g_part, sector_base, 0x1000) != ESP_OK) {
            free(tmp); return -1;
        }
        if (esp_partition_write(g_part, sector_base, tmp, 0x1000) != ESP_OK) {
            free(tmp); return -1;
        }
        free(tmp);
        written += chunk;
    }
    return (int32_t)bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    return true;
}

// --- FTP Server ---
FtpServer ftpServer;

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            break;
        default:
            break;
    }
}

static void setup_wifi(void) {
    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Failed to connect");
    }
}

static void setup_ffat(void) {
    Serial.println("[FFat] Mounting internal flash...");
    
    if (!FFat.begin(true)) {
        Serial.println("[FFat] ERROR: Failed to mount FFat!");
        return;
    }
    
    Serial.printf("[FFat] Total: %u bytes\n", FFat.totalBytes());
    Serial.printf("[FFat] Used: %u bytes\n", FFat.usedBytes());
    Serial.printf("[FFat] Free: %u bytes\n", FFat.freeBytes());
}

static void setup_usb_msc(void) {
    Serial.println("[USB] Setting up MSC...");
    
    g_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
    if (!g_part) {
        Serial.println("[USB] Partition 'ffat' not found!");
        return;
    }
    Serial.printf("[USB] Partition: offset=0x%x size=%u\n", g_part->address, g_part->size);
    
    uint32_t block_count = g_part->size / BLOCK_SIZE;
    
    MSC.vendorID("ESP32");
    MSC.productID("FLASH");
    MSC.productRevision("1.0");
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);
    MSC.onStartStop(onStartStop);
    MSC.mediaPresent(true);
    
    if (!MSC.begin(block_count, BLOCK_SIZE)) {
        Serial.println("[USB] MSC.begin failed!");
        return;
    }
    
    USB.begin();
    Serial.printf("[USB] MSC ready: %u blocks x %u bytes\n", block_count, BLOCK_SIZE);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 FTP + USB MSC (PR #2)    ║");
    Serial.println("║   FFat Flash + WiFi + USB              ║");
    Serial.println("╚══════════════════════════════════════════╝");
    
    setup_ffat();
    setup_usb_msc();
    setup_wifi();
    
    Serial.println("\n[FTP] Starting FTP server...");
    ftpServer.begin("esp32", "esp32");
    ftpServer.setLocalIp(WiFi.localIP());
    
    Serial.printf("[FTP] Ready at ftp://%s:%d\n", WiFi.localIP().toString().c_str(), FTP_PORT);
    Serial.println("[FTP] User: esp32 / Pass: esp32");
    Serial.println("\n[System] Setup complete - FTP + USB MSC running");
}

void loop() {
    static uint32_t lastTick = 0;
    
    ftpServer.handleFTP();
    delay(2);
    
    uint32_t now = millis();
    if (now - lastTick > 20000) {
        lastTick = now;
        Serial.printf("[Tick] FTP+MSC | IP: %s | RSSI: %d | Heap: %u | FFat free: %u\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap(), FFat.freeBytes());
    }
}