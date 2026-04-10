#pragma once

#include <Arduino.h>

struct sd_card_config_t {
    int sck;
    int miso;
    int mosi;
    int cs;
    uint32_t freq_hz;
    const char *mount_point;
};

bool sd_card_init(const sd_card_config_t *config);
void sd_card_deinit(void);
bool sd_card_is_ready(void);
uint8_t sd_card_type(void);
uint64_t sd_card_size_bytes(void);
uint32_t sd_card_sector_size(void);
uint32_t sd_card_num_sectors(void);
const char *sd_card_mount_point(void);
