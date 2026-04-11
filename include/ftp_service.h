#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct ftp_service_config_t
{
    const char *ftp_user;
    const char *ftp_password;
    uint16_t ftp_port;
};

// Initialize FTP service (call once at boot)
bool ftp_service_init(void);

// Start/stop FTP server
bool ftp_service_begin(const ftp_service_config_t *config);
void ftp_service_end(void);

// Poll FTP server (call in loop)
void ftp_service_poll(void);

// Query state
bool ftp_service_is_active(void);
bool ftp_service_is_client_connected(void);
IPAddress ftp_service_ip(void);

// Call this from main to sync MSC state with FTP service
void ftp_service_set_msc_enabled(bool enabled);
