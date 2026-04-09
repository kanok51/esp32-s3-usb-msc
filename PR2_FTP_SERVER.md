# PR #2: FTP Server with Internal Flash FAT Emulation

## Overview
Add FTP server capability to upload files, storing them on **internal SPIFFS flash** (no SD card required).

## Features
- **FTP Server** on port 21 (anonymous login)
- **WiFi Access Point** mode (ESP32-FTP / 12345678)
- **Internal Flash Storage** via SPIFFS (2MB partition at 0x610000)
- **USBMSC Compatible** - USB mass storage can run alongside

## Hardware
- ESP32-S3-DevKitC-1
- No additional wiring required

## WiFi Configuration
| Parameter | Value |
|-----------|-------|
| SSID | ESP32-FTP |
| Password | 12345678 |
| IP | 192.168.4.1 |
| FTP Port | 21 |
| Username | anonymous |
| Password | (empty) |

## FTP Commands Supported
- `LIST` / `NLST` - List files
- `RETR` - Download file
- `STOR` - Upload file
- `DELE` - Delete file
- `MKD` / `RMD` - Make/Remove directory
- `CWD` / `CDUP` - Change directory
- `PWD` - Print working directory

## Building & Upload
```bash
pio run -t upload
pio monitor
```

## Connection
```bash
# Via FTP client
ftp 192.168.4.1

# Or via browser
ftp://192.168.4.1
```

## Partition Layout
| Name | Type | Size | Purpose |
|------|------|------|---------|
| nvs | data | 20KB | NVS storage |
| otadata | data | 8KB | OTA data |
| app0 | app | 6MB | Main app |
| mscraw | data | 2MB | SPIFFS (FTP storage) |
| coredump | data | 64KB | Core dump |

## Known Limitations
1. **No TLS** - FTP is insecure, use only in trusted networks
2. **SPIFFS wear** - Frequent writes may wear flash; suitable for development
3. **Passive mode** - Not yet supported (active mode only)
4. **Memory** - Large file transfers may stress ESP32-S3 memory

## Future Enhancements
- [ ] FAT filesystem instead of SPIFFS
- [ ] TLS/SSL support (FTPS)
- [ ] User authentication
- [ ] Passive mode support
- [ ] BLE configuration interface
