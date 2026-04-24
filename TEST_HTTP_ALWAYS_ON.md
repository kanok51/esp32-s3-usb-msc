# Test Report: HTTP Always On + MSC Toggle Architecture

**Date:** 2026-04-24  
**Firmware Version:** HTTP Always On + MSC Toggle  
**Test Environment:** macOS, ESP32-S3 DevKitC

---

## Summary

**Status: ✅ ALL TESTS PASSED**

New architecture successfully implemented:
- **HTTP server ALWAYS running** (for control/status/mode switching)
- **MSC is a toggle** (enables/disables SD as USB drive)
- **SD access is mutually exclusive** (USB MSC OR HTTP, never both)

---

## Architecture Changes

### Before (Old)
```
Boot → MSC Mode (HTTP off)
     ↓
Serial 'h' → HTTP Mode (MSC off, HTTP on)
     ↓
Reset → MSC Mode (HTTP off)
```

### After (New)
```
Boot → MSC Mode (MSC on, HTTP running)
     ↓
HTTP /msc/off → MSC off (HTTP can access SD)
     ↓
HTTP /msc/on  → MSC on (SD as USB drive)
     ↓
Reset → MSC Mode (MSC on, HTTP running)
```

**Key Difference:** HTTP server never stops running!

---

## Test Results

### ✅ Test 1: HTTP Server Always Running
```
GET http://192.168.0.66/ → Status: 200 OK
```
**Status:** PASS - HTTP responds in all modes

---

### ✅ Test 2: MSC Enabled by Default
```
GET http://192.168.0.66/api/status
{
  "ip": "192.168.0.66",
  "msc_active": true,
  "sd_accessible": false,
  "wifi_connected": true
}
```
**Status:** PASS - MSC active on boot (SD as USB)

---

### ✅ Test 3: SD Blocked via HTTP when MSC Enabled
```
API shows: "sd_accessible": false when msc_active: true
Web UI shows: "⚠️ Upload disabled while MSC is active"
```
**Status:** PASS - Mutual exclusion working

---

### ✅ Test 4: MSC Disable via HTTP
```
GET http://192.168.0.66/msc/off
→ MSC disabled, HTTP can now access SD
```
**Status:** PASS - Mode switching works

---

### ✅ Test 5: HTTP Upload When MSC Disabled
```
POST http://192.168.0.66/upload with file
→ Status: 303 (redirect on success)
→ File appears in file list
```
**Status:** PASS - Uploads work when MSC off

---

### ✅ Test 6: HTTP Upload Blocked When MSC Enabled
```
POST http://192.168.0.66/upload (while MSC on)
→ Error: "Upload rejected: MSC mode is active"
```
**Status:** PASS - Uploads blocked when MSC on

---

### ✅ Test 7: USB MSC Access
```
ls /Volumes/TEST/
→ README.txt, System Volume Information, hello.txt, ...
```
**Status:** PASS - SD accessible as USB drive when MSC enabled

---

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Status page with upload form |
| `/upload` | POST | Upload file (blocked if MSC on) |
| `/delete?file=...` | GET | Delete file (blocked if MSC on) |
| `/msc/on` | GET | Enable MSC mode (SD as USB) |
| `/msc/off` | GET | Disable MSC mode (HTTP access) |
| `/api/status` | GET | JSON status: IP, MSC state, SD accessible |

---

## Serial Commands

| Command | Action |
|---------|--------|
| `m` | Enable MSC mode |
| `h` | Disable MSC mode |
| `s` | Show status |
| `r` | Reset settings |

---

## Web Interface

### MSC Mode Active
```
📱 MSC Mode Active
SD card is accessible as USB drive
Note: File upload via HTTP is currently disabled.

[Disable MSC (Enable HTTP Uploads)]  ← Button
```

### HTTP Mode Active (MSC Disabled)
```
🌐 HTTP Mode Active
SD card is accessible via HTTP uploads

Upload Form:
[Choose File] [Upload]

[Enable MSC (SD as USB Drive)]  ← Button
```

---

## Usage Examples

### Workflow: Upload via HTTP
```bash
# 1. Reset ESP32 (default: MSC enabled)
python3 scripts/reset_esp32.py --wait 15

# 2. Disable MSC via HTTP (enable uploads)
curl http://192.168.0.66/msc/off

# 3. Upload file
curl -X POST -F "file=@myfile.txt" http://192.168.0.66/upload

# 4. Re-enable MSC (SD as USB drive)
curl http://192.168.0.66/msc/on

# 5. SD card now accessible as USB drive
ls /Volumes/TEST/
```

### Check Status
```bash
# Get JSON status
curl http://192.168.0.66/api/status
# {"ip":"192.168.0.66","msc_active":false,"sd_accessible":true,"wifi_connected":true}
```

---

## Files Modified

| File | Changes |
|------|---------|
| `main.cpp` | HTTP always starts, MSC as toggle |
| `http_upload_service.cpp/h` | Always-on HTTP, MSC state tracking |
| `settings_store.cpp/h` | MSC mode persistence |
| `reset_esp32.py` | Updated for new architecture |
| `test_http_always_on.py` | New comprehensive test |

---

## Key Features

1. **HTTP Always Available**
   - Web UI always accessible
   - Mode switching via web
   - Status monitoring via API
   - No need for serial commands

2. **Safe Mutual Exclusion**
   - SD never accessed by both interfaces
   - HTTP uploads blocked when MSC on
   - Clear user feedback in web UI

3. **Persistent State**
   - MSC mode saved to flash
   - Survives reboots
   - Reset to MSC default via `r` command

4. **Multiple Control Methods**
   - Web UI buttons
   - HTTP API endpoints
   - Serial commands
   - Reset button (returns to MSC default)

---

## Test Script Usage

```bash
# Run comprehensive test
python3 scripts/test_http_always_on.py --mount-path /Volumes/TEST

# Quick reset and status
python3 scripts/reset_esp32.py --quiet --wait 15

# Expected output:
# IP: 192.168.0.66
# HTTP: http://192.168.0.66/
# MSC: enabled
```

---

## Conclusion

✅ **Architecture successfully implemented and tested**

**Benefits:**
1. HTTP always available for control/status
2. No serial commands needed for mode switching
3. Clear web interface for users
4. Safe mutual exclusion prevents SD corruption
5. Persistent mode across reboots

**Ready for production use!**
