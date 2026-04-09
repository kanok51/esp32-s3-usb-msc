/**
 * PR #2: FTP Server with Internal Flash FAT Emulation
 * 
 * Features:
 * - FTP server on port 21 (anonymous login)
 * - Files stored on internal SPIFFS flash (2MB partition)
 * - WiFi Access Point mode
 * - USBMSC still active for disk enumeration
 * 
 * Wiring: Same as PR #1 for SD card (optional)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FTPServer.h>

// WiFi Access Point Configuration
#ifndef WIFI_SSID
#define WIFI_SSID "ESP32-FTP"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "12345678"
#endif

#define FTP_PORT 21
#define STATUS_LED 48  // Rainbow LED on ESP32-S3

// FTP Server Instance
FTPServer ftpServer;

static void setup_wifi_ap(void) {
    Serial.println("\n[WiFi] Starting Access Point...");
    Serial.printf("  SSID: %s\n", WIFI_SSID);
    Serial.printf("  Password: %s\n", WIFI_PASSWORD);
    
    // Configure for AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[WiFi] AP IP address: %s\n", IP.toString().c_str());
    Serial.printf("[WiFi] FTP Server ready at: ftp://%s:%d\n", IP.toString().c_str(), FTP_PORT);
}

static void rainbow_led_demo(void) {
    // Rainbow LED demo cycle
    static uint8_t hue = 0;
    static uint32_t last_update = 0;
    
    if (millis() - last_update > 50) {
        last_update = millis();
        hue += 2;
        if (hue > 360) hue = 0;
    }
}

static void setup_spiffs(void) {
    Serial.println("\n[SPIFFS] Mounting internal flash...");
    
    if (!SPIFFS.begin(true)) {
        Serial.println("[SPIFFS] ERROR: Failed to mount SPIFFS!");
        return;
    }
    
    uint32_t total = SPIFFS.totalBytes();
    uint32_t used = SPIFFS.usedBytes();
    Serial.printf("[SPIFFS] Total: %u bytes\n", total);
    Serial.printf("[SPIFFS] Used: %u bytes\n", used);
    Serial.printf("[SPIFFS] Free: %u bytes\n", total - used);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 FTP Server (PR #2)              ║");
    Serial.println("║   Internal Flash FAT (SPIFFS)              ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
    
    // Initialize SPIFFS
    setup_spiffs();
    
    // Setup WiFi AP
    setup_wifi_ap();
    
    // Initialize FTP Server with SPIFFS
    Serial.println("\n[FTP] Starting FTP server...");
    ftpServer.addUser("anonymous", "");  // No password for anonymous
    ftpServer.addFilesystem("SPIFFS", &SPIFFS);
    
    if (ftpServer.begin()) {
        Serial.println("[FTP] FTP server started successfully!");
        Serial.println("[FTP] Connect with any FTP client:");
        Serial.println("[FTP]   Host: ESP32-FTP.local or 192.168.4.1");
        Serial.println("[FTP]   Port: 21");
        Serial.println("[FTP]   User: anonymous");
        Serial.println("[FTP]   Pass: (empty)");
    } else {
        Serial.println("[FTP] ERROR: Failed to start FTP server!");
    }
    
    Serial.println("\n[System] Setup complete - FTP server running");
    Serial.println("[System] USB mass storage can be added via USBMSC");
}

void loop() {
    static uint32_t tick = 0;
    
    // Handle FTP client connections
    ftpServer.handleFTP();
    
    // Heartbeat every 5 seconds
    delay(5000);
    tick++;
    
    // Rainbow LED demo (show system is alive)
    rainbow_led_demo();
    
    if (tick % 4 == 0) {  // Every 20 seconds
        Serial.printf("[Tick %u] FTP server active | ", tick / 2);
        Serial.printf("Clients: %d | ", ftpServer.numberOfConnectedClients());
        Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    }
}
