#pragma once

#include <Arduino.h>
#include <WiFi.h>

struct wifi_ftp_bridge_config_t
{
    const char *wifi_ssid;
    const char *wifi_password;
    const char *ftp_user;
    const char *ftp_password;
};

bool wifi_ftp_bridge_begin(const wifi_ftp_bridge_config_t *config);
void wifi_ftp_bridge_poll(void);

bool wifi_ftp_bridge_is_wifi_ready(void);
bool wifi_ftp_bridge_is_ftp_connected(void);
IPAddress wifi_ftp_bridge_ip(void);