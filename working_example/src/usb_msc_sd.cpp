#include "usb_msc_sd.h"

#include <Arduino.h>
#include <SD.h>
#include <USB.h>
#include <USBMSC.h>

static USBMSC g_msc;
static bool g_msc_started = false;
static bool g_usb_started = false;
static bool g_host_active = false;

static int32_t usb_msc_read_cb_impl(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    const uint32_t sector_size = SD.sectorSize();
    if (sector_size == 0 || offset != 0 || (bufsize % sector_size) != 0) {
        return -1;
    }

    uint8_t *dst = static_cast<uint8_t *>(buffer);
    const uint32_t sector_count = bufsize / sector_size;

    for (uint32_t i = 0; i < sector_count; ++i) {
        if (!SD.readRAW(dst + (i * sector_size), lba + i)) {
            return -1;
        }
    }

    g_host_active = true;
    return static_cast<int32_t>(bufsize);
}

static int32_t usb_msc_write_cb_impl(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    const uint32_t sector_size = SD.sectorSize();
    if (sector_size == 0 || offset != 0 || (bufsize % sector_size) != 0) {
        return -1;
    }

    const uint32_t sector_count = bufsize / sector_size;

    for (uint32_t i = 0; i < sector_count; ++i) {
        if (!SD.writeRAW(buffer + (i * sector_size), lba + i)) {
            return -1;
        }
    }

    g_host_active = true;
    return static_cast<int32_t>(bufsize);
}

static bool usb_msc_start_stop_cb_impl(uint8_t power_condition, bool start, bool load_eject)
{
    (void)power_condition;
    (void)start;

    if (load_eject) {
        g_host_active = false;
    }

    return true;
}

bool usb_msc_sd_begin(void)
{
    if (g_msc_started) {
        return true;
    }

    if (SD.cardType() == CARD_NONE) {
        return false;
    }

    g_msc.vendorID("VCC-GND");
    g_msc.productID("ESP32S3-SD");
    g_msc.productRevision("1.0");
    g_msc.onRead(usb_msc_read_cb_impl);
    g_msc.onWrite(usb_msc_write_cb_impl);
    g_msc.onStartStop(usb_msc_start_stop_cb_impl);

    g_msc.mediaPresent(true);

    if (!g_msc.begin(SD.numSectors(), SD.sectorSize())) {
        return false;
    }

    if (!g_usb_started) {
        USB.begin();
        g_usb_started = true;
    }

    g_msc_started = true;
    g_host_active = false;
    return true;
}

void usb_msc_sd_end(void)
{
    if (!g_msc_started) {
        return;
    }

    g_msc.mediaPresent(false);
    delay(250);

    g_msc.end();
    delay(150);

    g_msc_started = false;
    g_host_active = false;
}

bool usb_msc_sd_is_active(void)
{
    return g_msc_started;
}

bool usb_msc_sd_host_has_control(void)
{
    return g_host_active;
}