/**
 * PR #2: FTP Server with Internal Flash FAT Emulation
 * 
 * Features:
 * - FTP server on port 21 (anonymous login)
 * - Files stored on internal SPIFFS flash (2MB partition)
 * - WiFi Station mode - connects to home router
 * - USBMSC still active for disk enumeration
 * 
 * WiFi: pukushome_fritz / amarnetamikhabo
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FTPServer.h>

// WiFi Configuration (from platformio.ini build_flags)
#ifndef WIFI_SSID
#define WIFI_SSID "pukushome_fritz"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "amarnetamikhabo"
#endif

#define FTP_PORT 21
#define STATUS_LED 48  // Rainbow LED on ESP32-S3

// FTP Server Instance
FTPServer ftpServer;

// WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("[WiFi] Station started");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi] Connected to AP");
            break;
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

static void setup_wifi_station(void) {
    Serial.println("\n[WiFi] Connecting to station mode...");
    Serial.printf("  SSID: %s\n", WIFI_SSID);
    
    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] FTP Server ready at: ftp://%s:%d\n", WiFi.localIP().toString().c_str(), FTP_PORT);
    } else {
        Serial.println("[WiFi] Failed to connect - starting AP fallback");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-FTP", "12345678");
        Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
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
    Serial.println("║   WiFi Station + SPIFFS Flash             ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
    
    // Initialize SPIFFS
    setup_spiffs();
    
    // Setup WiFi Station
    setup_wifi_station();
    
    // Initialize FTP Server with SPIFFS
    Serial.println("\n[FTP] Starting FTP server...");
    ftpServer.addUser("anonymous", "");  // No password for anonymous
    ftpServer.addFilesystem("SPIFFS", &SPIFFS);
    
    if (ftpServer.begin()) {
        Serial.println("[FTP] FTP server started successfully!");
        Serial.println("[FTP] Connect with any FTP client:");
        Serial.printf("[FTP]   Host: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[FTP]   Port: %d\n", FTP_PORT);
        Serial.println("[FTP]   User: anonymous");
        Serial.println("[FTP]   Pass: (empty)");
    } else {
        Serial.println("[FTP] ERROR: Failed to start FTP server!");
    }
    
    Serial.println("\n[System] Setup complete - FTP server running");
}

void loop() {
    static uint32_t tick = 0;
    
    // Handle FTP client connections
    ftpServer.handleFTP();
    
    // Heartbeat every 5 seconds
    delay(5000);
    tick++;
    
    if (tick % 4 == 0) {  // Every 20 seconds
        Serial.printf("[Tick %u] FTP active | ", tick / 2);
        Serial.printf("IP: %s | ", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI: %d dBm | ", WiFi.RSSI());
        Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    }
}
