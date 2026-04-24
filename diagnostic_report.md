# SD Card FTP Upload Failure Diagnostic Report

## Issue Summary
FTP upload fails with "552 Probably insufficient storage space" error.

## Root Cause Analysis

### Finding: Bug in SimpleFTPServer Library

**Location:** `.pio/libdeps/esp32s3/SimpleFTPServer/FtpServer.h`, lines ~720-721

When `STORAGE_TYPE` is `STORAGE_SD` or `STORAGE_SD_MMC`:

```cpp
#elif STORAGE_TYPE == STORAGE_SD || STORAGE_TYPE == STORAGE_SD_MMC
  uint32_t capacity() { return true; };   // Returns 1 (boolean true)
  uint32_t free() { return true; };       // Returns 1 (boolean true)
```

### Problem
- `free()` returns `1` (byte), making the FTP server think there's virtually no space available
- `capacity()` also returns `1` (byte)
- When FTP client tries to upload any file, the server rejects it with "552 Probably insufficient storage space"

### Evidence in Library Code
From `FtpServer.cpp` line 1665:
```cpp
client.println(F("552 Probably insufficient storage space") );
```

This error is triggered when the FTP server thinks there's insufficient space for the upload.

## SD Card Status (from code review)

| Parameter | Status |
|-----------|--------|
| SD Card Library | Arduino SD (standard) |
| Mount Point | `/sd` |
| Card Detection | Working (code checks CARD_NONE) |
| Card Size Reading | Working (`SD.cardSize()`) |
| Sector Operations | Working (`readRAW`/`writeRAW` for MSC) |

The SD card initialization and MSC operations work correctly - the issue is **only** with FTP free space reporting.

## Comparison with Other Storage Types

From `FtpServer.h`:
- **SPIFFS/LittleFS**: Uses `totalBytes()` and `usedBytes()` - **works correctly**
- **SD/SD_MMC**: Returns `true` (1) - **BROKEN**
- **SdFat1/2**: Uses cluster calculations - **works correctly**
- **FFat**: Uses `totalBytes()` and `freeBytes()` - **works correctly**

## Solutions

### Option 1: Fix the SimpleFTPServer Library (Recommended)
Modify `FtpServer.h` to implement proper free space calculation for SD cards:

```cpp
#elif STORAGE_TYPE == STORAGE_SD || STORAGE_TYPE == STORAGE_SD_MMC
  uint32_t capacity() { 
      // Return total card size in bytes
      return (uint64_t)SD.cardSize() * SD.sectorSize();
  };
  uint32_t free() { 
      // For SD library, we don't have direct free space API
      // Return a large conservative estimate (90% of capacity)
      // or implement FAT calculation if needed
      uint64_t total = (uint64_t)SD.cardSize() * SD.sectorSize();
      return (uint32_t)(total * 0.9);  // Conservative estimate
  };
```

### Option 2: Use SdFat Library Instead
Switch from Arduino SD library to SdFat which has proper `freeClusterCount()` API. This requires changes to:
- `platformio.ini` - change library
- `sd_card.cpp` - use SdFat API

### Option 3: Patch Library Locally
Create a patch file or fork the library with the fix.

## Recommended Fix

**Option 1** is the quickest fix - modify the library header directly. However, since this is a PlatformIO downloaded library, the fix will be lost on clean build.

**Better approach**: Add the library to the project with the fix, or use `lib_deps` with a forked version.

### Immediate Workaround
Add this to `ftp_service.cpp` before `#include <FtpServer.h>`:

```cpp
// Override the broken SD free/space functions before including FtpServer
#define FREE_SPACE_HACK
```

Unfortunately, since these are inline methods in the header, we need to patch the library itself.

### Permanent Fix Steps

1. **Fork SimpleFTPServer** or create a local copy in `lib/SimpleFTPServer/`
2. **Fix `FtpServer.h`** lines 720-721 with proper SD size calculations
3. **Update `platformio.ini`** to use local library:
   ```ini
   lib_deps = 
       ${PROJECT_DIR}/lib/SimpleFTPServer
   ```

## Verification Test Plan

1. Build and upload firmware
2. Connect to serial monitor - verify SD card initializes
3. Connect to FTP server
4. Try uploading a file
5. Expected: Upload succeeds
6. Expected: `FTP_FREE_SPACE_CHANGE` callback reports correct values

## Notes

- The standard Arduino SD library doesn't expose FAT free space calculation easily
- The card size is accurate but free space requires FAT table walking
- For most use cases, returning 90% of card capacity as "free" is sufficient
- If exact free space is needed, must use SdFat library

## References

- SimpleFTPServer v3.0.2 source: https://github.com/xreef/SimpleFTPServer
- Arduino SD Library: https://www.arduino.cc/en/reference/SD
- SdFat Library (alternative): https://github.com/greiman/SdFat
