#include "sd_card.h"

#include <SPI.h>
#include <SD.h>

static SPIClass g_spi(FSPI);
static sd_card_config_t g_cfg = {};
static bool g_ready = false;

bool sd_card_init(const sd_card_config_t *config)
{
    if (config == nullptr || config->mount_point == nullptr) {
        Serial.println("[SD] ERROR: Invalid config");
        return false;
    }

    // If already initialized, deinit first
    if (g_ready) {
        sd_card_deinit();
    }

    g_cfg = *config;

    Serial.printf("[SD] Initializing SPI SCK=%d MISO=%d MOSI=%d CS=%d\n",
                  g_cfg.sck, g_cfg.miso, g_cfg.mosi, g_cfg.cs);

    g_spi.begin(g_cfg.sck, g_cfg.miso, g_cfg.mosi, g_cfg.cs);
    pinMode(g_cfg.cs, OUTPUT);
    digitalWrite(g_cfg.cs, HIGH);
    delay(20);

    if (!SD.begin(g_cfg.cs, g_spi, g_cfg.freq_hz, g_cfg.mount_point)) {
        Serial.println("[SD] ERROR: SD.begin() failed");
        g_ready = false;
        return false;
    }

    if (SD.cardType() == CARD_NONE) {
        Serial.println("[SD] ERROR: No SD card detected");
        SD.end();
        g_ready = false;
        return false;
    }

    g_ready = true;

    uint8_t ct = SD.cardType();
    const char *type_str = "UNKNOWN";
    switch (ct) {
        case CARD_MMC:  type_str = "MMC"; break;
        case CARD_SD:   type_str = "SDSC"; break;
        case CARD_SDHC: type_str = "SDHC/SDXC"; break;
        default: break;
    }

    Serial.printf("[SD] Card type: %s\n", type_str);
    Serial.printf("[SD] Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
    Serial.printf("[SD] Sector size: %u bytes\n", SD.sectorSize());
    Serial.printf("[SD] Sector count: %u\n", SD.numSectors());
    Serial.printf("[SD] Mount point: %s\n", g_cfg.mount_point);
    Serial.println("[SD] Initialized successfully");

    return true;
}

void sd_card_deinit(void)
{
    if (g_ready) {
        SD.end();
        Serial.println("[SD] Deinitialized");
    }
    g_ready = false;
}

bool sd_card_is_ready(void)
{
    return g_ready;
}

uint8_t sd_card_type(void)
{
    if (!g_ready) {
        return CARD_NONE;
    }
    return SD.cardType();
}

uint64_t sd_card_size_bytes(void)
{
    if (!g_ready) {
        return 0;
    }
    return SD.cardSize();
}

uint32_t sd_card_sector_size(void)
{
    if (!g_ready) {
        return 0;
    }
    return SD.sectorSize();
}

uint32_t sd_card_num_sectors(void)
{
    if (!g_ready) {
        return 0;
    }
    return SD.numSectors();
}

const char *sd_card_mount_point(void)
{
    return g_cfg.mount_point;
}