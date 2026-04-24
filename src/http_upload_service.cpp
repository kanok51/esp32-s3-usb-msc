/**
 * HTTP Upload Service
 * Always-running HTTP server with MSC-controlled SD access
 */

#include "http_upload_service.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

static WebServer* g_server = nullptr;
static http_upload_config_t g_cfg;
static bool g_initialized = false;
static bool g_msc_active = false;  // When true, HTTP cannot access SD
static bool g_msc_enable_requested = false;
static bool g_msc_disable_requested = false;

// File handle for upload
static File g_upload_file;

// Helper: Check if SD is accessible via HTTP (MSC must be off)
static bool sd_accessible(void) {
    return !g_msc_active;
}

static String get_html_header(const char* title) {
    String html = "<!DOCTYPE html\u003e\n";
    html += "\u003chtml\u003e\u003chead\u003e\n";
    html += "\u003cmeta charset='UTF-8'\u003e\n";
    html += "\u003cmeta name='viewport' content='width=device-width, initial-scale=1'\u003e\n";
    html += "\u003ctitle\u003e" + String(title) + "\u003c/title\u003e\n";
    html += "\u003cstyle\u003e";
    html += "body{font-family:Arial,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5}";
    html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px}";
    html += ".status{background:#fff;padding:15px;border-radius:8px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
    html += ".status.msc{background:#e3f2fd;border-left:4px solid #2196F3}";
    html += ".status.http{background:#e8f5e9;border-left:4px solid #4CAF50}";
    html += ".upload-form{background:#fff;padding:20px;border-radius:8px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
    html += ".file-list{background:#fff;padding:15px;border-radius:8px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
    html += "button{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin:5px}";
    html += "button:hover{background:#45a049}";
    html += "button.danger{background:#f44336}";
    html += "button.danger:hover{background:#da190b}";
    html += "button.msc{background:#2196F3}";
    html += "button.msc:hover{background:#1976D2}";
    html += "input[type=file]{margin:10px 0}";
    html += "table{width:100%;border-collapse:collapse;margin-top:10px}";
    html += "th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd}";
    html += "th{background:#4CAF50;color:white}";
    html += ".disabled{color:#999;font-style:italic}";
    html += ".note{background:#fff3cd;padding:10px;border-radius:4px;margin:10px 0}";
    html += "a{color:#4CAF50;text-decoration:none}";
    html += "a:hover{text-decoration:underline}";
    html += "\u003c/style\u003e\n";
    html += "\u003c/head\u003e\u003cbody\u003e\n";
    return html;
}

static String get_html_footer(void) {
    String html = "\u003chr\u003e\n";
    html += "\u003cp\u003e\u003csmall\u003eESP32-S3 SD Interface | IP: " + WiFi.localIP().toString() + "\u003c/small\u003e\u003c/p\u003e\n";
    html += "\u003c/body\u003e\u003c/html\u003e";
    return html;
}

static void send_redirect(const char* location) {
    g_server->sendHeader("Location", location);
    g_server->send(303);
}

static void handle_root() {
    String html = get_html_header("ESP32-S3 SD Interface");
    
    // Status section
    html += "\u003ch1\u003eSD Card Interface\u003c/h1\u003e\n";
    
    if (g_msc_active) {
        html += "\u003cdiv class='status msc'\u003e\n";
        html += "\u003ch3\u003e📱 MSC Mode Active\u003c/h3\u003e\n";
        html += "\u003cp\u003e\u003cstrong\u003eSD card is accessible as USB drive\u003c/strong\u003e\u003c/p\u003e\n";
        html += "\u003cp\u003eConnect the ESP32 to your computer via USB.\u003cbr\u003e";
        html += "The SD card will appear as a removable drive.\u003c/p\u003e\n";
        html += "\u003cdiv class='note'\u003e\n";
        html += "\u003cstrong\u003eNote:\u003c/strong\u003e File upload via HTTP is currently disabled.\u003cbr\u003e";
        html += "Disable MSC mode to enable HTTP uploads.\n";
        html += "\u003c/div\u003e\n";
        html += "\u003c/div\u003e\n";
    } else {
        html += "\u003cdiv class='status http'\u003e\n";
        html += "\u003ch3\u003e🌐 HTTP Mode Active\u003c/h3\u003e\n";
        html += "\u003cp\u003e\u003cstrong\u003eSD card is accessible via HTTP uploads\u003c/strong\u003e\u003c/p\u003e\n";
        html += "\u003cp\u003eUse the upload form below to add files to the SD card.\u003c/p\u003e\n";
        html += "\u003c/div\u003e\n";
    }
    
    // Mode toggle buttons
    html += "\u003cdiv class='status'\u003e\n";
    html += "\u003ch3\u003eMode Control\u003c/h3\u003e\n";
    if (g_msc_active) {
        html += "\u003cp\u003eCurrent: \u003cstrong\u003eMSC Mode\u003c/strong\u003e (SD as USB drive)\u003c/p\u003e\n";
        html += "\u003ca href='/msc/off'\u003e\u003cbutton class='danger'\u003eDisable MSC (Enable HTTP Uploads)\u003c/button\u003e\u003c/a\u003e\n";
    } else {
        html += "\u003cp\u003eCurrent: \u003cstrong\u003eHTTP Mode\u003c/strong\u003e (Upload via web)\u003c/p\u003e\n";
        html += "\u003ca href='/msc/on'\u003e\u003cbutton class='msc'\u003eEnable MSC (SD as USB Drive)\u003c/button\u003e\u003c/a\u003e\n";
    }
    html += "\u003c/div\u003e\n";
    
    // Upload section (only when MSC is off)
    html += "\u003cdiv class='upload-form'\u003e\n";
    html += "\u003ch3\u003eUpload File\u003c/h3\u003e\n";
    if (g_msc_active) {
        html += "\u003cp class='disabled'\u003e⚠️ Upload disabled while MSC is active.\u003cbr\u003e";
        html += "Disable MSC mode above to enable uploads.\u003c/p\u003e\n";
    } else {
        html += "\u003cform method='POST' action='/upload' enctype='multipart/form-data'\u003e\n";
        html += "\u003cinput type='file' name='file' required\u003e\u003cbr\u003e\n";
        html += "\u003cbutton type='submit'\u003eUpload\u003c/button\u003e\n";
        html += "\u003c/form\u003e\n";
    }
    html += "\u003c/div\u003e\n";
    
    // File list
    html += "\u003cdiv class='file-list'\u003e\n";
    html += "\u003ch3\u003eFiles on SD Card\u003c/h3\u003e\n";
    
    if (g_msc_active) {
        html += "\u003cp class='disabled'\u003e⚠️ File list unavailable while MSC is active.\u003cbr\u003e";
        html += "Please use your computer's file manager to access files via USB.\u003c/p\u003e\n";
    } else if (!SD.begin()) {
        html += "\u003cp class='disabled'\u003e⚠️ SD card not accessible.\u003c/p\u003e\n";
    } else {
        File root = SD.open("/");
        if (!root) {
            html += "\u003cp\u003eError opening SD card\u003c/p\u003e\n";
        } else {
            html += "\u003ctable\u003e\n";
            html += "\u003ctr\u003e\u003cth\u003eName\u003c/th\u003e\u003cth\u003eSize\u003c/th\u003e\u003cth\u003eAction\u003c/th\u003e\u003c/tr\u003e\n";
            
            File entry;
            int count = 0;
            while ((entry = root.openNextFile()) && count < 50) {
                String name = entry.name();
                if (!name.startsWith(".")) {
                    html += "\u003ctr\u003e";
                    html += "\u003ctd\u003e" + name + "\u003c/td\u003e";
                    html += "\u003ctd\u003e" + String(entry.size()) + " bytes\u003c/td\u003e";
                    html += "\u003ctd\u003e\u003ca href='/delete?file=" + name + "' onclick='return confirm(\"Delete " + name + "?\")'\u003e\u003cbutton class='danger'\u003eDelete\u003c/button\u003e\u003c/a\u003e\u003c/td\u003e";
                    html += "\u003c/tr\u003e\n";
                }
                entry.close();
                count++;
            }
            root.close();
            
            if (count == 0) {
                html += "\u003ctr\u003e\u003ctd colspan='3'\u003eNo files found\u003c/td\u003e\u003c/tr\u003e\n";
            }
            html += "\u003c/table\u003e\n";
        }
    }
    html += "\u003c/div\u003e\n";
    
    html += get_html_footer();
    g_server->send(200, "text/html", html);
}

static void handle_upload() {
    HTTPUpload& upload = g_server->upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        // Check if MSC is active at start of upload
        if (g_msc_active) {
            Serial.println("[HTTP] Upload rejected - MSC active");
            return; // Will send error in final handler
        }
        
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        Serial.printf("[HTTP] Upload start: %s\n", filename.c_str());
        
        // Remove existing file
        if (SD.exists(filename)) {
            SD.remove(filename);
        }
        
        // Open file for writing
        g_upload_file = SD.open(filename, FILE_WRITE);
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write data chunk
        if (g_upload_file) {
            g_upload_file.write(upload.buf, upload.currentSize);
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        // Close file and redirect
        if (g_upload_file) {
            g_upload_file.close();
            Serial.printf("[HTTP] Upload complete: %s (%d bytes)\n", 
                upload.filename.c_str(), upload.totalSize);
        }
        send_redirect("/");
        
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        // Upload was aborted
        if (g_upload_file) {
            g_upload_file.close();
        }
        Serial.println("[HTTP] Upload aborted");
    }
}

static void handle_upload_result() {
    // Check if MSC is active - if so, reject upload
    if (g_msc_active) {
        g_server->send(403, "text/plain", 
            "Upload rejected: MSC mode is active.\n\n"
            "The SD card is currently mounted as a USB drive.\n"
            "Please disable MSC mode to enable HTTP uploads.\n\n"
            "Visit http://" + WiFi.localIP().toString() + "/ to switch modes."
        );
        return;
    }
    
    // Upload was handled by handle_upload, now redirect
    send_redirect("/");
}

static void handle_delete() {
    // Check if MSC is active - if so, reject delete
    if (g_msc_active) {
        g_server->send(403, "text/plain", 
            "Delete rejected: MSC mode is active.\n\n"
            "The SD card is currently mounted as a USB drive.\n"
            "Please disable MSC mode or use your computer's file manager."
        );
        return;
    }
    
    if (g_server->hasArg("file")) {
        String filename = g_server->arg("file");
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        
        Serial.printf("[HTTP] Delete: %s\n", filename.c_str());
        
        if (SD.exists(filename)) {
            SD.remove(filename);
            g_server->sendHeader("Location", "/");
            g_server->send(303);
        } else {
            g_server->send(404, "text/plain", "File not found");
        }
    } else {
        g_server->send(400, "text/plain", "Missing file parameter");
    }
}

static void handle_msc_on() {
    Serial.println("[HTTP] MSC enable requested via web");
    g_msc_enable_requested = true;
    
    String html = get_html_header("Enable MSC Mode");
    html += "\u003ch1\u003eMSC Mode Enabled\u003c/h1\u003e\n";
    html += "\u003cp\u003eMSC mode is being enabled...\u003c/p\u003e\n";
    html += "\u003cp\u003eThe SD card will be accessible as a USB drive.\u003c/p\u003e\n";
    html += "\u003cscript\u003esetTimeout(function(){window.location.href='/';}, 3000);\u003c/script\u003e\n";
    html += get_html_footer();
    g_server->send(200, "text/html", html);
}

static void handle_msc_off() {
    Serial.println("[HTTP] MSC disable requested via web");
    g_msc_disable_requested = true;
    
    String html = get_html_header("Disable MSC Mode");
    html += "\u003ch1\u003eMSC Mode Disabled\u003c/h1\u003e\n";
    html += "\u003cp\u003eMSC mode is being disabled...\u003c/p\u003e\n";
    html += "\u003cp\u003eHTTP uploads are now enabled.\u003c/p\u003e\n";
    html += "\u003cscript\u003esetTimeout(function(){window.location.href='/';}, 3000);\u003c/script\u003e\n";
    html += get_html_footer();
    g_server->send(200, "text/html", html);
}

static void handle_api_status() {
    String json = "{";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"msc_active\":" + String(g_msc_active ? "true" : "false") + ",";
    json += "\"sd_accessible\":" + String(sd_accessible() ? "true" : "false") + ",";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += "}";
    g_server->send(200, "application/json", json);
}

bool http_upload_init(void) {
    if (g_initialized) {
        return true;
    }
    
    Serial.println("[HTTP] Service initialized");
    g_initialized = true;
    return true;
}

bool http_upload_begin(const http_upload_config_t* config) {
    if (!config) {
        Serial.println("[HTTP] ERROR: Invalid config");
        return false;
    }
    
    g_cfg = *config;
    if (g_cfg.server_port == 0) {
        g_cfg.server_port = 80;
    }
    
    // Check if WiFi is already connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[HTTP] ERROR: WiFi not connected");
        return false;
    }
    
    Serial.println("[HTTP] Using existing WiFi connection");
    
    // Create server
    g_server = new WebServer(g_cfg.server_port);
    
    // Setup routes
    g_server->on("/", HTTP_GET, handle_root);
    g_server->on("/upload", HTTP_POST, handle_upload_result, handle_upload);
    g_server->on("/delete", HTTP_GET, handle_delete);
    g_server->on("/msc/on", HTTP_GET, handle_msc_on);
    g_server->on("/msc/off", HTTP_GET, handle_msc_off);
    g_server->on("/api/status", HTTP_GET, handle_api_status);
    
    g_server->begin();
    
    Serial.printf("[HTTP] Server started on port %d\n", g_cfg.server_port);
    Serial.printf("[HTTP] URL: http://%s/\n", WiFi.localIP().toString().c_str());
    
    return true;
}

void http_upload_poll(void) {
    if (g_server) {
        g_server->handleClient();
    }
}

String http_upload_get_ip(void) {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "Not Connected";
}

bool http_upload_is_running(void) {
    return (g_server != nullptr);
}

void http_upload_set_msc_active(bool active) {
    g_msc_active = active;
    Serial.printf("[HTTP] MSC state: %s\n", active ? "ACTIVE (SD blocked)" : "INACTIVE (SD accessible)");
}

bool http_upload_should_enable_msc(void) {
    if (g_msc_enable_requested) {
        g_msc_enable_requested = false;
        return true;
    }
    return false;
}

bool http_upload_should_disable_msc(void) {
    if (g_msc_disable_requested) {
        g_msc_disable_requested = false;
        return true;
    }
    return false;
}

// Legacy compatibility
bool http_upload_is_active(void) {
    return http_upload_is_running();
}

void http_upload_end(void) {
    // No-op - HTTP server always runs
    Serial.println("[HTTP] Ignoring end() - server always runs");
}
