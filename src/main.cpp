/**
 * PR #1: SD Card Initialization using Software Bit-Banged SPI
 * 
 * Goal: Initialize SD card with full control over pins - no driver conflicts.
 * Uses bit-banged SPI which is slower but 100% reliable.
 * 
 * Wiring:
 *   CS   -> GPIO 47
 *   MISO -> GPIO 48
 *   MOSI -> GPIO 45
 *   SCK  -> GPIO 21
 */

#include <Arduino.h>

#define SD_CS_PIN   47
#define SD_MISO_PIN 48
#define SD_MOSI_PIN 45
#define SD_CLK_PIN  21

// Bit-banged SPI macros
#define SPI_SCK_LOW()  digitalWrite(SD_CLK_PIN, LOW)
#define SPI_SCK_HIGH() digitalWrite(SD_CLK_PIN, HIGH)
#define SPI_MOSI_HIGH() digitalWrite(SD_MOSI_PIN, HIGH)
#define SPI_MOSI_LOW()  digitalWrite(SD_MOSI_PIN, LOW)
#define SPI_READ_MISO() digitalRead(SD_MISO_PIN)
#define SPI_CS_LOW()  digitalWrite(SD_CS_PIN, LOW)
#define SPI_CS_HIGH() digitalWrite(SD_CS_PIN, HIGH)

static void spi_init_pins(void) {
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(SD_MISO_PIN, INPUT);
    pinMode(SD_MOSI_PIN, OUTPUT);
    pinMode(SD_CLK_PIN, OUTPUT);
    SPI_CS_HIGH();
    SPI_SCK_LOW();
    
    // Brief delay to let voltages stabilize
    delay(100);
}

static uint8_t spi_transfer(uint8_t data) {
    uint8_t recv = 0;
    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i)) {
            SPI_MOSI_HIGH();
        } else {
            SPI_MOSI_LOW();
        }
        delayMicroseconds(5);
        SPI_SCK_HIGH();
        delayMicroseconds(5);
        if (SPI_READ_MISO()) {
            recv |= (1 << i);
        }
        SPI_SCK_LOW();
        delayMicroseconds(5);
    }
    return recv;
}

static void sd_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t* response, int resp_len) {
    SPI_CS_LOW();
    spi_transfer(0xFF);
    spi_transfer(cmd | 0x40);
    spi_transfer(arg >> 24);
    spi_transfer(arg >> 16);
    spi_transfer(arg >> 8);
    spi_transfer(arg);
    spi_transfer(crc);
    
    for (int i = 0; i < resp_len; i++) {
        response[i] = spi_transfer(0xFF);
    }
    SPI_CS_HIGH();
}

static bool sd_reset(void) {
    Serial.println("[SD] Sending SD reset sequence...");
    
    SPI_CS_HIGH();
    for (int i = 0; i < 80; i++) spi_transfer(0xFF);
    
    uint8_t response[5];
    for (int attempt = 0; attempt < 10; attempt++) {
        sd_command(0, 0, 0x95, response, 1);
        Serial.printf("[SD] CMD0 attempt %d: R1=0x%02X\n", attempt + 1, response[0]);
        if (response[0] == 0x01 || response[0] == 0x00) {
            Serial.printf("[SD] Got valid R1 response: 0x%02X (attempt %d)\n", response[0], attempt + 1);
            break;
        }
        delay(10);
    }
    
    if (response[0] != 0x01 && response[0] != 0x00) {
        Serial.printf("[SD] ERROR: No valid response from SD card: 0x%02X\n", response[0]);
        return false;
    }
    
    Serial.println("[SD] Card is in idle state!");
    
    uint8_t ocr[4];
    sd_command(58, 0, 0x95, ocr, 4);
    Serial.printf("[SD] CMD58 OCR: 0x%02X%02X%02X%02X\n", ocr[0], ocr[1], ocr[2], ocr[3]);
    
    Serial.println("[SD] Sending CMD1 to init card...");
    for (int attempt = 0; attempt < 10; attempt++) {
        sd_command(1, 0x40000000, 0x95, response, 1);
        Serial.printf("[SD] CMD1 attempt %d: R1=0x%02X\n", attempt + 1, response[0]);
        if (response[0] == 0x00) {
            Serial.println("[SD] Card ready! R1=0x00");
            break;
        }
        if (response[0] != 0x01 && response[0] != 0x00) {
            Serial.printf("[SD] Unexpected response: 0x%02X\n", response[0]);
        }
        delay(10);
    }
    
    if (response[0] == 0x00) {
        Serial.printf("[SD] SUCCESS: SD card initialized! R1: 0x%02X\n", response[0]);
        return true;
    }
    
    Serial.println("[SD] ERROR: Card did not become ready");
    return false;
}

static bool sd_read_cid(void) {
    uint8_t cid[16];
    SPI_CS_LOW();
    spi_transfer(0xFF);
    spi_transfer(0x40 | 10);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    
    for (int i = 0; i < 16; i++) {
        cid[i] = spi_transfer(0xFF);
    }
    SPI_CS_HIGH();
    spi_transfer(0xFF);
    
    Serial.println("\n[SD] CID:");
    Serial.printf("  Manufacturer: 0x%02X\n", cid[0]);
    Serial.printf("  OEM: %c%c\n", cid[1], cid[2]);
    Serial.printf("  Name: %c%c%c%c%c\n", cid[3], cid[4], cid[5], cid[6], cid[7]);
    Serial.printf("  Serial: 0x%02X%02X%02X%02X\n", cid[8], cid[9], cid[10], cid[11]);
    
    return true;
}

static bool sd_readSector(uint32_t sector, uint8_t* buffer) {
    uint8_t response[5];
    
    SPI_CS_LOW();
    spi_transfer(0xFF);
    spi_transfer(0x40 | 17);
    spi_transfer(sector >> 24);
    spi_transfer(sector >> 16);
    spi_transfer(sector >> 8);
    spi_transfer(sector);
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    
    int timeout = 1000;
    while (spi_transfer(0xFF) != 0x00) {
        if (timeout-- <= 0) {
            Serial.println("[SD] ERROR: Read sector timeout (no data token)");
            SPI_CS_HIGH();
            return false;
        }
    }
    
    while (spi_transfer(0xFF) != 0xFE);
    
    for (int i = 0; i < 512; i++) {
        buffer[i] = spi_transfer(0xFF);
    }
    
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    
    SPI_CS_HIGH();
    spi_transfer(0xFF);
    
    return true;
}

bool init_sd_card(void) {
    Serial.println("\n[SD] Initializing SD card with bit-banged SPI...");
    Serial.printf("  CS Pin:   GPIO %d\n", SD_CS_PIN);
    Serial.printf("  MISO Pin: GPIO %d\n", SD_MISO_PIN);
    Serial.printf("  MOSI Pin: GPIO %d\n", SD_MOSI_PIN);
    Serial.printf("  CLK Pin:  GPIO %d\n", SD_CLK_PIN);
    
    spi_init_pins();
    
    // Read raw MISO to check if card is pulling it high
    int miso_raw = digitalRead(SD_MISO_PIN);
    Serial.printf("[SD] MISO pin state before init: %d\n", miso_raw);
    
    if (!sd_reset()) {
        Serial.println("[SD] FATAL: SD card reset failed!");
        return false;
    }
    
    if (!sd_read_cid()) {
        Serial.println("[SD] WARNING: Could not read CID");
    }
    
    uint8_t block[512];
    if (sd_readSector(0, block)) {
        Serial.println("[SD] SUCCESS: Read sector 0!");
        Serial.printf("  Byte 510: 0x%02X, Byte 511: 0x%02X\n", block[510], block[511]);
        if (block[510] == 0x55 && block[511] == 0xAA) {
            Serial.println("[SD] Valid boot sector signature!");
        }
        return true;
    }
    
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 SD Card Test (PR #1)      ║");
    Serial.println("║   Bit-Banged SPI Mode               ║");
    Serial.println("╚════════════════════════════════════════════╝");
    
    if (!init_sd_card()) {
        Serial.println("\n[SD] FATAL: SD card initialization failed!");
        while (true) {
            delay(3000);
            Serial.println("[SD] Retrying SD card init...");
            if (init_sd_card()) break;
        }
    } else {
        Serial.println("[SD] SD card is operational!");
    }
}

void loop() {
    static uint32_t tick = 0;
    delay(2000);
    Serial.printf("[Tick %u] SD Card active\n", tick++);
}
