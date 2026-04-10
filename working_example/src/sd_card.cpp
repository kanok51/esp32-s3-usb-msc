#include "sd_card.h"

#include <SPI.h>
#include <SD.h>

static SPIClass g_spi(FSPI);
static sd_card_config_t g_cfg = {};
static bool g_ready = false;

bool sd_card_init(const sd_card_config_t *config)
{
    if (config == nullptr || config->mount_point == nullptr) {
        return false;
    }

    g_cfg = *config;

    g_spi.begin(g_cfg.sck, g_cfg.miso, g_cfg.mosi, g_cfg.cs);
    pinMode(g_cfg.cs, OUTPUT);
    digitalWrite(g_cfg.cs, HIGH);
    delay(20);

    if (!SD.begin(g_cfg.cs, g_spi, g_cfg.freq_hz, g_cfg.mount_point)) {
        g_ready = false;
        return false;
    }

    if (SD.cardType() == CARD_NONE) {
        SD.end();
        g_ready = false;
        return false;
    }

    g_ready = true;
    return true;
}

void sd_card_deinit(void)
{
    if (g_ready) {
        SD.end();
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
