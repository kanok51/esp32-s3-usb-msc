#pragma once

#include <Arduino.h>

// Initialize USB MSC exposing the SD card as read-only
bool usb_msc_sd_begin(void);

// Disable USB MSC
void usb_msc_sd_end(void);

// Refresh: disable, delay, re-enable (host sees updated files)
bool usb_msc_sd_refresh(void);

// Query state
bool usb_msc_sd_is_active(void);
bool usb_msc_sd_host_has_control(void);
