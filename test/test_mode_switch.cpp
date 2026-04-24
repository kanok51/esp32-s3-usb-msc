/**
 * Test: MSC/FTP Mode Switching
 * 
 * Verifies:
 * 1. MSC can start and stop properly
 * 2. FTP can start after MSC stops
 * 3. Mode switching doesn't corrupt the filesystem
 * 4. SD card remains accessible after mode switches
 */

#include <Arduino.h>
#include <unity.h>
#include <SD.h>
#include <WiFi.h>

#include "config.h"
#include "sd_card.h"
#include "usb_msc_sd.h"
#include "ftp_service.h"

// Test configuration
#define TEST_WIFI_SSID APP_WIFI_SSID
#define TEST_WIFI_PASS APP_WIFI_PASSWORD
#define TEST_FILE "/sd/test_mode_switch.txt"

static sd_card_config_t sd_cfg = {
    .sck = APP_SD_SCK,
    .miso = APP_SD_MISO,
    .mosi = APP_SD_MOSI,
    .cs = APP_SD_CS,
    .freq_hz = APP_SD_FREQ_HZ,
    .mount_point = APP_SD_MOUNT_POINT
};

void setUp(void) {
    // Runs before each test
}

void tearDown(void) {
    // Runs after each test
}

// ============== TEST 1: MSC can start ==============
void test_msc_can_start(void) {
    TEST_ASSERT_TRUE(sd_card_init(&sd_cfg));
    TEST_ASSERT_TRUE(usb_msc_sd_begin());
    TEST_ASSERT_TRUE(usb_msc_sd_is_active());
    Serial.println("[TEST] MSC started successfully");
}

// ============== TEST 2: MSC can stop ==============
void test_msc_can_stop(void) {
    TEST_ASSERT_TRUE(usb_msc_sd_is_active());
    usb_msc_sd_end();
    delay(300);
    TEST_ASSERT_FALSE(usb_msc_sd_is_active());
    Serial.println("[TEST] MSC stopped successfully");
}

// ============== TEST 3: FTP can start after MSC stops ==============
void test_ftp_can_start_after_msc_stops(void) {
    // Ensure MSC is stopped
    if (usb_msc_sd_is_active()) {
        usb_msc_sd_end();
        delay(300);
    }
    
    // Remount SD for filesystem
    SD.end();
    delay(150);
    TEST_ASSERT_TRUE(sd_card_init(&sd_cfg));
    delay(100);
    
    // Connect WiFi (required for FTP)
    WiFi.mode(WIFI_STA);
    WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASS);
    
    uint32_t timeout = 30000;
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        if (millis() - start > timeout) {
            TEST_FAIL_MESSAGE("WiFi connection timeout");
            return;
        }
    }
    
    // Initialize and start FTP
    ftp_service_init();
    
    ftp_service_config_t ftp_cfg = {
        .ftp_user = "test",
        .ftp_password = "test",
        .ftp_port = 21
    };
    
    TEST_ASSERT_TRUE(ftp_service_begin(&ftp_cfg));
    TEST_ASSERT_TRUE(ftp_service_is_active());
    Serial.println("[TEST] FTP started after MSC stopped");
}

// ============== TEST 4: Filesystem integrity after mode switch ==============
void test_filesystem_integrity_after_mode_switch(void) {
    const char *test_data = "Mode switch test data - 12345";
    char read_buffer[64] = {0};
    
    // Write test file before mode switch
    File writeFile = SD.open(TEST_FILE, FILE_WRITE);
    TEST_ASSERT_TRUE(writeFile);
    writeFile.print(test_data);
    writeFile.close();
    Serial.println("[TEST] Test file written");
    
    // Switch to MSC mode
    ftp_service_end();
    SD.end();
    delay(300);
    
    TEST_ASSERT_TRUE(usb_msc_sd_begin());
    TEST_ASSERT_TRUE(usb_msc_sd_is_active());
    Serial.println("[TEST] Switched to MSC mode");
    
    // Switch back to FTP mode
    usb_msc_sd_end();
    delay(300);
    SD.end();
    delay(150);
    TEST_ASSERT_TRUE(sd_card_init(&sd_cfg));
    delay(100);
    Serial.println("[TEST] Switched back to FTP mode");
    
    // Verify file integrity
    File readFile = SD.open(TEST_FILE, FILE_READ);
    TEST_ASSERT_TRUE(readFile);
    int bytesRead = readFile.read((uint8_t*)read_buffer, sizeof(read_buffer) - 1);
    read_buffer[bytesRead] = '\0';
    readFile.close();
    
    TEST_ASSERT_EQUAL_STRING(test_data, read_buffer);
    Serial.println("[TEST] File integrity verified after mode switch");
    
    // Cleanup
    SD.remove(TEST_FILE);
}

// ============== TEST 5: SD access after multiple switches ==============
void test_sd_access_after_multiple_switches(void) {
    const int SWITCH_COUNT = 3;
    
    for (int i = 0; i < SWITCH_COUNT; i++) {
        Serial.printf("[TEST] Switch cycle %d/%d\n", i + 1, SWITCH_COUNT);
        
        // Stop MSC/FTP, ensure clean state
        if (usb_msc_sd_is_active()) {
            usb_msc_sd_end();
            delay(300);
        }
        ftp_service_end();
        SD.end();
        delay(200);
        
        // Switch to MSC mode
        TEST_ASSERT_TRUE(sd_card_init(&sd_cfg));
        TEST_ASSERT_TRUE(usb_msc_sd_begin());
        TEST_ASSERT_TRUE(usb_msc_sd_is_active());
        delay(100);
        
        // Stop MSC
        usb_msc_sd_end();
        delay(300);
        
        // Switch to FTP mode
        SD.end();
        delay(150);
        TEST_ASSERT_TRUE(sd_card_init(&sd_cfg));
        delay(100);
        
        // Verify SD filesystem access
        File testFile = SD.open("/sd/stress_test.txt", FILE_WRITE);
        TEST_ASSERT_TRUE(testFile);
        testFile.printf("Cycle %d", i);
        testFile.close();
        
        SD.remove("/sd/stress_test.txt");
    }
    
    Serial.println("[TEST] Multiple mode switches completed successfully");
}

// ============== SETUP & LOOP ==============
void setup() {
    Serial.begin(115200);
    delay(3000);  // Wait for serial monitor
    
    Serial.println("\n========================================");
    Serial.println("  Mode Switching Test Suite");
    Serial.println("========================================\n");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_msc_can_start);
    RUN_TEST(test_msc_can_stop);
    RUN_TEST(test_ftp_can_start_after_msc_stops);
    RUN_TEST(test_filesystem_integrity_after_mode_switch);
    RUN_TEST(test_sd_access_after_multiple_switches);
    
    UNITY_END();
}

void loop() {
    // Nothing here - tests run in setup()
}
