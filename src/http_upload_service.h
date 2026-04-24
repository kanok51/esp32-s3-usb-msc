/**
 * HTTP Upload Service
 * HTTP server that ALWAYS runs, with SD access controlled by MSC state
 * When MSC is active: HTTP shows status, no SD access
 * When MSC is inactive: HTTP can access SD for uploads
 */

#pragma once

#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

// Configuration
struct http_upload_config_t {
    const char* wifi_ssid;
    const char* wifi_password;
    int server_port;
};

// Initialize HTTP upload service
bool http_upload_init(void);

// Begin HTTP server (WiFi should already be connected)
bool http_upload_begin(const http_upload_config_t* config);

// Process HTTP requests (call in loop)
void http_upload_poll(void);

// Get IP address
String http_upload_get_ip(void);

// Check if HTTP server is running
bool http_upload_is_running(void);

// Set MSC active state (controls SD access)
// When MSC is active, HTTP cannot access SD
void http_upload_set_msc_active(bool active);

// Check if MSC should be enabled (via HTTP request)
bool http_upload_should_enable_msc(void);

// Check if MSC should be disabled (via HTTP request)
bool http_upload_should_disable_msc(void);

// Legacy compatibility (deprecated)
bool http_upload_is_active(void);
void http_upload_end(void);
