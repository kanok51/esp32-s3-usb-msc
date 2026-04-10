#pragma once

#include <Arduino.h>

bool usb_msc_sd_begin(void);
void usb_msc_sd_end(void);

bool usb_msc_sd_is_active(void);
bool usb_msc_sd_host_has_control(void);