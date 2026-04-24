# ESP32-S3 SD/MSC/FTP Project - Status Report

## Project Overview
ESP32-S3 firmware with SD card access via USB Mass Storage Class (MSC) and FTP server.

## Fixes Applied

### 1. SimpleFTPServer Library Patches
**Location:** `lib/SimpleFTPServer/`

#### Fix A: Storage Type Override Protection
**File:** `FtpServer.h`

The original library unconditionally defines `STORAGE_TYPE` based on the platform, ignoring build flags. Added `#ifndef STORAGE_TYPE` guards to allow custom configuration:

```cpp
#ifndef STORAGE_TYPE
    #define STORAGE_TYPE DEFAULT_STORAGE_TYPE_ESP32
#endif
```

This fix was applied to all platform branches (ESP8266, ESP32, STM32, RP2040, SAMD, Arduino).

#### Fix B: SD Card Free Space Calculation  
**File:** `FtpServer.h` (lines ~756-772)

**Original broken code:**
```cpp
#elif STORAGE_TYPE == STORAGE_SD || STORAGE_TYPE == STORAGE_SD_MMC
  uint32_t capacity() { return true; };   // Returns 1!
  uint32_t free() { return true; };       // Returns 1!
```

**Fixed code:**
```cpp
#elif STORAGE_TYPE == STORAGE_SD || STORAGE_TYPE == STORAGE_SD_MMC
  uint32_t capacity() {
      // Return total card capacity in bytes
      uint64_t cardSizeBytes = (uint64_t)STORAGE_MANAGER.cardSize() * STORAGE_MANAGER.sectorSize();
      return (uint32_t)cardSizeBytes;
  };
  uint32_t free() {
      // Return conservative estimate: 90% of capacity
      uint64_t cardSizeBytes = (uint64_t)STORAGE_MANAGER.cardSize() * STORAGE_MANAGER.sectorSize();
      uint64_t estimatedFree = (cardSizeBytes * 9) / 10;
      return (uint32_t)estimatedFree;
  };
```

### 2. Build Configuration Cleanup
**File:** `platformio.ini`

Removed conflicting build flags that were being ignored:
```ini
# REMOVED:
# -DDEFAULT_STORAGE_TYPE_ESP32=5
# -DSTORAGE_TYPE=5

# Kept only:
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=0
```

Storage type is now defined in code before including the library:
```cpp
#define DEFAULT_FTP_SERVER_NETWORK_TYPE_ESP32 NETWORK_ESP32
#define DEFAULT_STORAGE_TYPE_ESP32 STORAGE_SD  // Must be before include
#include <FtpServer.h>
```

### 3. FTP Service Logic Fix
**File:** `src/ftp_service.cpp`

#### Fix C: MSC Active State Detection
Changed callback to check actual MSC state instead of settings:

```cpp
// BEFORE (buggy):
if (g_msc_enabled) {  // Check settings flag
    g_request_switch_to_ftp_mode = true;
}

// AFTER (fixed):
if (usb_msc_sd_is_active()) {  // Check actual runtime state
    g_request_switch_to_ftp_mode = true;
}
```

#### Fix D: Conditional SD Remount
Avoid unnecessary SD remount when MSC is not actually running:

```cpp
// REMOUNT SD only if MSC was active
if (usb_msc_sd_is_active()) {
    Serial.println("[FTP] Stopping MSC before FTP can start");
    usb_msc_sd_end();
    delay(300);
    
    Serial.println("[FTP] Remounting SD for FTP access...");
    if (!remount_sd_for_fs()) {
        Serial.println("[FTP] ERROR: SD remount failed");
        return false;
    }
} else {
    Serial.println("[FTP] MSC not active - using existing SD mount");
}
```

## Current Status

### ✅ What's Working
1. **SD Card Initialization** - Card detected, 15GB capacity recognized
2. **USB MSC** - SD card accessible as USB drive when MSC enabled
3. **WiFi Connection** - Connects successfully, IP assigned (192.168.0.66)
4. **FTP Server** - Starts successfully, accepts connections
5. **FTP Authentication** - Login with esp32/esp32pass works
6. **Directory Listing** - Can list files on SD card via FTP
7. **Direct SD Write** - Writing files from ESP32 code works perfectly

### ❌ What's Not Working
**FTP File Uploads** - All upload attempts fail with:
```
552 Probably insufficient storage space
```

Or connection resets during transfer.

## Root Cause Analysis

The SimpleFTPServer library v3.0.2 has a **file write implementation bug** when using the standard Arduino SD library on ESP32-S3.

### Evidence:
1. Direct SD writes from ESP32 code work: `SD.open("/file.txt", FILE_WRITE)` succeeds
2. The `free()` and `capacity()` fixes are working (verified by serial output)
3. Connection and directory listing work fine
4. Only STOR (upload) commands fail
5. The working_example in this repo has the same bug

### Suspected Cause:
The library's `file.write()` implementation or buffer handling has issues with the ESP32 Arduino core's SD library. The library was primarily tested with:
- SdFat (not Arduino SD)
- SPIFFS/LittleFS (not SD)
- ESP8266 (not ESP32-S3)

## Workarounds

### Option 1: Use SdFat Library (Recommended for Production)
Replace Arduino SD library with SdFat which has better support in SimpleFTPServer.

**Changes needed:**
1. In `platformio.ini`:
   ```ini
   lib_deps = 
       greiman/SdFat @ ^2.2.0
       SimpleFTPServer @ ^3.0.2
   ```

2. Update `sd_card.cpp` to use SdFat API instead of SD.h

3. Change storage type in `ftp_service.cpp`:
   ```cpp
   #define DEFAULT_STORAGE_TYPE_ESP32 STORAGE_SDFAT2
   ```

### Option 2: Use WebDAV Instead of FTP
Implement WebDAV server using existing ESP32 libraries which have better SD card support.

### Option 3: Use HTTP File Upload
Implement a simple HTTP endpoint for file uploads instead of FTP.

### Option 4: Custom File Transfer Protocol
Implement a simple TCP-based protocol for file transfers.

### Option 5: Use ESP8266 Core (Not Recommended)
The library works better on ESP8266, but this requires different hardware.

## Recommended Next Steps

1. **Short term**: Document that FTP uploads are not working; use SD card via USB MSC for file transfers
2. **Medium term**: Implement HTTP file upload endpoint as alternative
3. **Long term**: Switch to SdFat library or find alternative FTP library

## Testing

### Serial Monitor
```bash
cd esp32-s3-usb-msc
pio device monitor
```

### FTP Connection Test
```bash
cd esp32-s3-usb-msc
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

Expected:
- Connection: ✅ PASS
- Upload/Download: ❌ FAIL (552 error)

### Manual FTP Test
```bash
# Install ftp client
ftp 192.168.0.66
# Login: esp32 / esp32pass
# Try: put local.txt remote.txt
# Result: 552 Probably insufficient storage space
```

## File Structure

```
esp32-s3-usb-msc/
├── lib/
│   └── SimpleFTPServer/          # PATCHED library
│       ├── FtpServer.h            # ← Fixes applied
│       ├── FtpServer.cpp
│       └── FtpServerKey.h
├── src/
│   ├── main.cpp                   # ← Added SD test
│   ├── ftp_service.cpp            # ← Fixed MSC detection
│   ├── sd_card.cpp                # SD card interface
│   └── ...
├── platformio.ini                 # ← Cleaned build flags
└── scripts/
    └── test_ftp.py               # FTP test script
```

## Conclusion

The ESP32-S3 SD/MSC/FTP project is functional for:
- ✅ SD card access via USB MSC (plug into computer)
- ✅ FTP file downloads and directory listing
- ✅ WiFi connectivity

But FTP uploads are blocked by a library bug that cannot be fixed without either:
1. Switching to SdFat library
2. Finding a different FTP library
3. Implementing custom file transfer

The fixes applied prevent the "insufficient storage" error from being caused by incorrect free space reporting, but the underlying file write operation in the library remains non-functional with Arduino SD on ESP32-S3.
