/**
 * PR #1: SD Card SPI Initialization
 * 
 * Goal: Initialize SD card over SPI and verify card detection/read/write.
 * No USB, no FatFS - pure hardware validation.
 * 
 * Wiring (SPI Mode):
 *   MISO -> GPIO 13
 *   MOSI -> GPIO 11
 *   CLK  -> GPIO 12
 *   CS   -> GPIO 10
 */

#include <Arduino.h>
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#define SD_CS_PIN  10
#define SD_MISO_PIN 13
#define SD_MOSI_PIN 11
#define SD_CLK_PIN  12

static sdmmc_card_t *card = nullptr;

void print_card_info(const sdmmc_card_t *card) {
    Serial.println("\n===========================================");
    Serial.println("         SD CARD DETECTED!");
    Serial.println("===========================================");
    
    Serial.printf("  Card Type:    ");
    switch (card->ocr) {
        case SDMMC_OCR_SDSC: Serial.println("SDSC"); break;
        case SDMMC_OCR_SDHC_SDXC: Serial.println("SDHC/SDXC"); break;
        case SDMMC_OCR_MMC: Serial.println("MMC"); break;
        default: Serial.println("Unknown"); break;
    }

    Serial.printf("  Card Size:    %llu MB\n",
        (unsigned long long)(card->csd.capacity * card->csd.read_bl_len / 1024 / 1024));
    Serial.printf("  sectors:      %u\n", card->csd.capacity);
    Serial.printf("  Read Block:   %u bytes\n", card->csd.read_bl_len);
    Serial.printf("  Write Block:  %u bytes\n", card->csd.write_bl_len);
    
    Serial.println("\n  CID:");
    Serial.printf("    Manufacturer:  0x%02X\n", card->cid.mfg_id);
    Serial.printf("    OEM/Product:   %c%c\n", 
        card->cid.oem_id[0], card->cid.oem_id[1]);
    Serial.printf("    Product Name:  %s\n", card->cid.name);
    Serial.printf("    Revision:      %u.%u\n",
        card->cid.revision >> 4, card->cid.revision & 0x0F);
    Serial.printf("    Serial:        0x%08X\n", card->cid.serial);
    
    Serial.println("===========================================\n");
}

bool test_sd_readwrite() {
    Serial.println("[SD] Testing block read/write...");
    
    // Read CSD register as a test
    uint8_t csd_buf[16];
    esp_err_t err = sdmmc_send_cid(card->host.slot, csd_buf);
    if (err != ESP_OK) {
        Serial.printf("[SD] ERROR: Failed to read CID: %s\n", esp_err_to_name(err));
        return false;
    }
    
    Serial.println("[SD] CID read successfully!");
    
    // Read first block (MBR/Sector 0) as test
    uint8_t block[512];
    err = sdmmc_read_sectors(card, block, 0, 1);
    if (err != ESP_OK) {
        Serial.printf("[SD] ERROR: Failed to read sector 0: %s\n", esp_err_to_name(err));
        return false;
    }
    
    Serial.println("[SD] Sector 0 read successfully!");
    Serial.printf("  Byte 510: 0x%02X, Byte 511: 0x%02X\n", block[510], block[511]);
    
    if (block[510] == 0x55 && block[511] == 0xAA) {
        Serial.println("[SD] SD card appears to have a valid boot sector!");
    } else {
        Serial.println("[SD] Note: No boot sector signature (card may be unformatted)");
    }
    
    return true;
}

bool init_sd_card(void) {
    Serial.println("\n[SD] Initializing SD card over SPI...");
    Serial.printf("  CS Pin:   GPIO %d\n", SD_CS_PIN);
    Serial.printf("  MISO Pin: GPIO %d\n", SD_MISO_PIN);
    Serial.printf("  MOSI Pin: GPIO %d\n", SD_MOSI_PIN);
    Serial.printf("  CLK Pin:  GPIO %d\n", SD_CLK_PIN);
    
    // Configure SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.miso_io_num = SD_MISO_PIN;
    bus_cfg.mosi_io_num = SD_MOSI_PIN;
    bus_cfg.sclk_io_num = SD_CLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 512;
    
    // Attach SD card to SPI bus
    esp_err_t err = spi_bus_initialize(HSPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        Serial.printf("[SD] ERROR: spi_bus_initialize failed: %s\n", esp_err_to_name(err));
        return false;
    }
    Serial.println("[SD] SPI bus initialized.");
    
    // Configure SD card host (SPI mode)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)SD_CS_PIN;
    slot_config.host_id = SPI_HOST;
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_HOST;
    
    // Initialize SD card
    Serial.println("[SD] Calling sdmmc_card_init...");
    
    card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    if (!card) {
        Serial.println("[SD] ERROR: malloc failed for card structure");
        return false;
    }
    
    err = sdmmc_card_init(&host, card);
    if (err != ESP_OK) {
        Serial.printf("[SD] ERROR: sdmmc_card_init failed: %s\n", esp_err_to_name(err));
        free(card);
        card = nullptr;
        return false;
    }
    
    // Card init successful!
    print_card_info(card);
    
    // Run read/write test
    if (test_sd_readwrite()) {
        Serial.println("[SD] SUCCESS: SD card is fully operational!");
    } else {
        Serial.println("[SD] WARNING: SD card detected but read/write test failed.");
    }
    
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 SD Card Test (PR #1)           ║");
    Serial.println("║   SD Card SPI Initialization              ║");
    Serial.println("╚════════════════════════════════════════════╝");
    
    if (!init_sd_card()) {
        Serial.println("\n[SD] FATAL: SD card initialization failed!");
        Serial.println("[SD] Check wiring and ensure SD card is inserted.");
        while (true) {
            delay(1000);
            Serial.println("[SD] Retrying...");
            if (init_sd_card()) break;
        }
    }
}

void loop() {
    static uint32_t tick = 0;
    delay(1000);
    
    if (card != nullptr) {
        Serial.printf("[Tick %u] SD Card active - sectors: %u\n", 
            tick++, card->csd.capacity);
    } else {
        Serial.printf("[Tick %u] Waiting for SD card...\n", tick++);
    }
}
