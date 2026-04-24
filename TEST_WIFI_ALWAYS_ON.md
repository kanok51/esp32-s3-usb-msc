# Test Report: WiFi Always On + No Auto-Switch

**Date:** 2026-04-24  
**Firmware Version:** MSC-Default with WiFi Always On, No Auto-Switch

---

## Summary

**Status: ✅ ALL TESTS PASSED**

WiFi starts in ALL modes (MSC and HTTP). Mode switching requires:
1. **Manual reset** (returns to MSC default)
2. **Serial command** ('h' for HTTP, 'm' for MSC)
3. **Web UI button** (stops HTTP, requires reset to complete)

Auto-switch to MSC after upload has been REMOVED.

---

## Changes Made

### 1. WiFi Always On ✅
- WiFi connects during boot (both MSC and HTTP modes)
- IP address always available after reset
- `reset_esp32.py` displays IP in all cases

### 2. No Auto-Switch ✅
- Removed automatic MSC switch from main loop
- User must explicitly reset or use serial command
- Web UI "Switch to MSC" button now instructs user to reset

### 3. Mode Switching Methods

| Method | Action | Result |
|--------|--------|--------|
| Reset button | Press or run `reset_esp32.py` | Returns to MSC mode |
| Serial 'h' | Send via serial monitor | Switch to HTTP mode |
| Serial 'm' | Send via serial monitor | Switch to MSC mode |
| Web UI button | Click "Switch to MSC" | Stops HTTP, requires reset |

---

## Test Results

### Test 1: MSC Boot with WiFi ✅

```
[WiFi] Connected, IP: 192.168.0.66
[Main] WiFi ready
[Main] Default: MSC mode
========================================
  Current Mode: MSC
  WiFi IP: 192.168.0.66
========================================
```

**Status:** PASS - WiFi connects, IP visible

---

### Test 2: HTTP Mode with WiFi ✅

```
[HTTP] Using existing WiFi connection
========================================
  Current Mode: HTTP
  WiFi IP: 192.168.0.66
========================================

HTTP Mode Active:
  - Web interface for file uploads
  - WiFi connected + HTTP server ON
```

**Status:** PASS - WiFi stays connected, HTTP server starts

---

### Test 3: Reset Script Shows IP ✅

```bash
python3 scripts/reset_esp32.py --wait 15
```

**Output:**
```
==================================================
BOOT SUMMARY
==================================================
Detected Mode: MSC
WiFi IP Address: 192.168.0.66
==================================================
```

**Status:** PASS - IP always displayed

---

### Test 4: Web UI Mode Switch ✅

When user clicks "Switch to MSC" button:

```
MSC Mode Switch Requested

The HTTP server will now stop.
SD card released for MSC mode.

TO COMPLETE THE SWITCH:
1. Reset the ESP32 (unplug/replug or use reset button)
2. Or run: python3 scripts/reset_esp32.py

After reset, the SD card will appear as a USB drive.
```

**Status:** PASS - Clear instructions, no auto-switch

---

### Test 5: Serial Commands ✅

| Command | Behavior |
|---------|----------|
| `h` | Switch to HTTP mode (if in MSC) |
| `m` | Switch to MSC mode (if in HTTP) |
| `r` | Reset settings to default |

**Status:** PASS - All commands work

---

## Workflow Example

### Upload File via HTTP (Standard Workflow)

```bash
# 1. Reset to MSC mode (default)
python3 scripts/reset_esp32.py --wait 15
# Output: Mode: MSC, IP: 192.168.0.66

# 2. Switch to HTTP mode
python3 -c "import serial; s=serial.Serial('...',115200); s.write(b'h'); s.close()"
sleep 10

# 3. Upload file
curl -X POST -F "file=@myfile.txt" http://192.168.0.66/upload

# 4. Reset to return to MSC mode
python3 scripts/reset_esp32.py --wait 15
# SD card now accessible as USB drive
```

---

## Code Changes

### Modified Files:

| File | Change |
|------|--------|
| `main.cpp` | WiFi always starts, removed auto-switch from loop |
| `http_upload_service.cpp` | Web UI stops HTTP on MSC request |
| `http_upload_service.h` | Added manual switch request function |
| `reset_esp32.py` | Parses WiFi IP from MSC boot |

---

## Serial Output Examples

### MSC Mode (After Reset)
```
========================================
  ESP32-S3 SD Card Interface
  WiFi Always On + MSC/HTTP Modes
========================================

[Main] Starting WiFi (always on)...
[WiFi] Connected, IP: 192.168.0.66
[Main] WiFi ready
[Main] Default: MSC mode

========================================
  Current Mode: MSC
  WiFi IP: 192.168.0.66
========================================

MSC Mode Active:
  - SD card accessible as USB drive
  - WiFi connected (for status/management)
  - HTTP server OFF (prevents SD corruption)

To switch to HTTP mode (upload via web):
  Send 'h' via serial monitor

To reset ESP32 (returns to MSC mode):
  Run: python3 scripts/reset_esp32.py
```

### HTTP Mode (After 'h' Command)
```
[Serial] Command received: 'h'
[Main] Switching to HTTP Mode...
[HTTP] Using existing WiFi connection
[HTTP] Server started on port 80
[HTTP] URL: http://192.168.0.66/

========================================
  HTTP Mode Active
========================================
  URL: http://192.168.0.66/
  Mode: HTTP
  WiFi IP: 192.168.0.66

HTTP Mode Active:
  - Web interface for file uploads
  - SD card NOT accessible via USB
  - WiFi connected + HTTP server ON

To switch back to MSC mode:
  Click 'Switch to MSC Mode' in web interface
  OR reset the ESP32
```

---

## Key Differences from Previous Version

| Feature | Before | Now |
|---------|--------|-----|
| WiFi in MSC | ❌ No | ✅ Yes |
| IP after reset | ❌ N/A | ✅ Always shown |
| Auto-switch MSC | ✅ Automatic | ❌ Manual reset |
| Web UI switch | ✅ Instant | ⚠️ Requires reset |
| Serial 'm' | ✅ Works | ✅ Works |

---

## Conclusion

✅ **Implementation complete and tested**

**Working Features:**
1. WiFi always on in both modes ✅
2. IP address visible after every reset ✅
3. No unwanted auto-switch behavior ✅
4. Clear mode switching instructions ✅
5. Mutual exclusion maintained (HTTP/MSC) ✅
6. Serial commands functional ✅

**Recommended User Flow:**
1. Reset ESP32 → MSC mode + WiFi connected
2. Note IP from reset output
3. Send 'h' via serial → HTTP mode
4. Upload files via web interface
5. Reset ESP32 → Back to MSC mode
