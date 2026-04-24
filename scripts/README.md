# ESP32-S3 Scripts

This folder contains automation and testing scripts for the ESP32-S3 SD/MSC/FTP project.

## Quick Reference - NEW HTTP+MSC Scripts

| Script | Description | Usage |
|--------|-------------|-------|
| `reset_esp32.py` ⭐ | **Reset ESP32** | `python3 reset_esp32.py` |
| `upload_file.py` ⭐ | **Simple file upload** | `python3 upload_file.py myfile.txt` |
| `test_workflow.py` | Full workflow test | `python3 test_workflow.py --mount-path /Volumes/TEST` |
| `test_negative_*.py` | Mutual exclusion tests | See below |
| `test_all.py` | Run all tests | `python3 test_all.py --mount-path /Volumes/TEST` |

## HTTP+MSC Dual Interface Scripts

### reset_esp32.py ⭐ UTILITY
**Purpose:** Reset ESP32 via serial DTR/RTS and show IP address from boot log  
**Use when:** You need to restart the ESP32 (returns to MSC default mode)

```bash
# Simple reset + show boot summary (default)
python3 scripts/reset_esp32.py

# Reset with custom port
python3 scripts/reset_esp32.py --port /dev/cu.usbmodem101

# Reset and monitor boot for 15 seconds
python3 scripts/reset_esp32.py --wait 15

# Reset and keep monitoring (like serial monitor)
python3 scripts/reset_esp32.py --monitor
# Press Ctrl+C to exit monitor

# Quiet reset (minimal output, just IP)
python3 scripts/reset_esp32.py --quiet

# Output includes:
# - Detected Mode (MSC or HTTP)
# - WiFi IP Address (if HTTP mode)
# - Web Interface URL (if HTTP mode)
```

**Features:**
- Quick reset back to MSC default mode
- Parses boot log for WiFi IP address (WiFi starts in ALL modes)
- Shows current mode (MSC or HTTP)
- If in HTTP mode, displays IP and upload URL
- If in MSC mode, shows instructions to switch to HTTP
- Can monitor boot messages

**Mode Switching Note:** After HTTP upload, you MUST reset the ESP32 to return to MSC mode. The web UI "Switch to MSC" button stops the HTTP server but requires a reset to complete.
- Works when ESP32 is stuck in HTTP mode

### upload_file.py ⭐ RECOMMENDED
**Purpose:** Simple standalone file upload via WiFi/HTTP  
**Use when:** You just need to upload a file quickly

```bash
# Upload a file (simplest way)
python3 scripts/upload_file.py mydata.txt

# Upload with custom IP
python3 scripts/upload_file.py mydata.txt --ip 192.168.1.100

# Upload with different remote name
python3 upload_file.py local.txt --remote-name device_data.txt

# Quiet mode (no output except errors)
python3 upload_file.py myfile.txt --quiet

# IMPORTANT: After upload, RESET ESP32 to return to MSC mode
python3 scripts/reset_esp32.py --wait 15
# (Web UI "Switch to MSC" button also works but requires reset)
```

## 📋 Complete Workflow

### HTTP Upload Workflow (Recommended)
```bash
# Step 1: Reset ESP32 (returns to MSC default mode)
python3 scripts/reset_esp32.py --wait 15
# Note the IP address shown (e.g., 192.168.0.66)

# Step 2: Switch to HTTP mode via serial
python3 -c "import serial; s=serial.Serial('/dev/cu.usbmodem...',115200); s.write(b'h'); s.close()"
sleep 10  # Wait for HTTP server to start

# Step 3: Upload your files
curl -X POST -F "file=@mydata.txt" http://192.168.0.66/upload
# OR use: python3 scripts/upload_file.py mydata.txt

# Step 4: IMPORTANT - Reset to return to MSC mode
python3 scripts/reset_esp32.py --wait 15
# SD card now appears as USB drive
```

### Mode Switching Notes
- **MSC Mode (Default):** SD card as USB drive, WiFi connected, no HTTP server
- **HTTP Mode:** Web upload interface, SD card NOT accessible via USB, WiFi + HTTP active
- **To switch MSC → HTTP:** Send 'h' via serial OR use web button in MSC mode
- **To switch HTTP → MSC:** Reset ESP32 (returns to default) OR send 'm' via serial
- **Web UI button:** Stops HTTP server but requires reset to complete MSC startup

## 🔄 Mode Switch Commands

| Command | Method | Result |
|---------|--------|--------|
| Reset button | Physical or `reset_esp32.py` | MSC mode (default) |
| Serial 'h' | `python3 -c "import serial; ... s.write(b'h')"` | HTTP mode |
| Serial 'm' | `python3 -c "import serial; ... s.write(b'm')"` | MSC mode |
| Web UI | Click "Switch to MSC Mode" | HTTP stops, needs reset |

### test_workflow.py
**Purpose:** Complete workflow test  
**What it does:**
1. Uploads test file via HTTP
2. Clicks "Switch to MSC Mode" button
3. Waits for MSC mount
4. Verifies file is readable via MSC
5. Reports results

```bash
# Full test (requires ESP32 on same machine via USB)
python3 scripts/test_workflow.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST

# HTTP-only test (no MSC verification)
python3 scripts/test_workflow.py --esp32-ip 192.168.0.66
```

### test_negative_msc_while_http.py
**Purpose:** Verify MSC is blocked while HTTP is active  
**What it proves:**
- MSC mount is unavailable when HTTP is running
- Prevents SD card corruption
- Mutual exclusion is working

```bash
# Run with ESP32 in HTTP mode
python3 scripts/test_negative_msc_while_http.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST

# Expected: TEST PASSED (MSC unavailable)
```

### test_negative_http_while_msc.py
**Purpose:** Verify HTTP is blocked while MSC is active  
**What it proves:**
- HTTP server is OFF when MSC is running
- HTTP upload fails when MSC is mounted
- Device A can safely read from MSC

```bash
# First: Switch ESP32 to MSC mode (via web interface)
# Wait for mount to appear
# Then run:
python3 scripts/test_negative_http_while_msc.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST

# Expected: TEST PASSED (HTTP unavailable)
```

### test_all.py
**Purpose:** Run all tests as a suite  

```bash
# Run entire test suite
python3 scripts/test_all.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST

# Skip negative tests (faster)
python3 scripts/test_all.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST --skip-negative

# Run specific tests
python3 scripts/test_all.py --esp32-ip 192.168.0.66 --mount-path /Volumes/TEST --tests upload,workflow
```

## Test Requirements

All HTTP+MSC test scripts require:

```bash
pip install requests
```

## Typical Usage Flow

```bash
cd esp32-s3-usb-msc

# 1. Upload files via HTTP
python3 scripts/upload_file.py sensor_data.csv
python3 scripts/upload_file.py config.json

# 2. Open browser, click "Switch to MSC Mode"
# http://192.168.0.66/

# 3. Wait for MSC mount, then plug into Device A
# Device A reads files from USB drive

# 4. When done, restart ESP32 to return to HTTP mode
```

## Original Scripts (Build/Deploy)

| Script | Description | Usage |
|--------|-------------|-------|
| `esp32_workflow.py` | Full workflow automation ⭐ | `python3 esp32_workflow.py` |
| `workflow.sh` | Bash version | `../workflow.sh` |

---

## Recommended: Python Workflow Script

The `esp32_workflow.py` script provides the most reliable and feature-complete workflow.

### Basic Usage

```bash
# From project root
cd esp32-s3-usb-msc

# Full workflow (build → upload → reset → test)
python3 scripts/esp32_workflow.py

# Skip build (use existing firmware)
python3 scripts/esp32_workflow.py --skip-build

# Custom IP address
python3 esp32_workflow.py --ip 192.168.1.100

# Only monitor serial output
python3 scripts/esp32_workflow.py --monitor-only

# Get help
python3 scripts/esp32_workflow.py --help
```

### Advanced Usage

```bash
# Reset and test only (no build/upload)
python3 scripts/esp32_workflow.py --skip-build --reset --test

# Custom serial port
python3 scripts/esp32_workflow.py --port /dev/cu.usbmodem101

# Custom project directory
python3 scripts/esp32_workflow.py --project-dir /path/to/project
```

## Legacy FTP Test Script

`test_ftp.py` - Original test script, still works but less automated.

```bash
# Basic test
python3 test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass

# Longer timeout
python3 test_ftp.py --host 192.168.0.66 --timeout 60
```

**Expected Output:**
```
Testing FTP on 192.168.0.66

=== Test: FTP Connection ===
Connecting to 192.168.0.66...
Connected successfully
Result: PASS

=== Test: FTP Upload/Download ===
FTP error: 552 Probably insufficient storage space
Result: FAIL

Overall: SOME TESTS FAILED
```

⚠️ The upload failure is expected due to a library bug. Use USB MSC instead.

## Bash Workflow Script

`workflow.sh` is the bash version (must be run from project root).

```bash
# From project root
cd esp32-s3-usb-msc
./workflow.sh

# Skip build
./workflow.sh --skip-build

# Custom IP
./workflow.sh --ip 192.168.1.100
```

## Python API Usage

You can also use the workflow module programmatically:

```python
#!/usr/bin/env python3
from scripts.esp32_workflow import ESP32Workflow

# Create workflow instance
workflow = ESP32Workflow(
    project_dir='/path/to/esp32-s3-usb-msc',
    ftp_ip='192.168.0.66',
    serial_port='/dev/cu.usbmodem53140032081',
)

# Run individual steps
workflow.check_prerequisites()
workflow.build_and_upload()
workflow.reset_board()
workflow.wait_for_boot()
results = workflow.run_ftp_tests()

print(f"Connection: {'OK' if results['connection'] else 'FAILED'}")
print(f"Upload: {'OK' if results['upload'] else 'FAILED (expected)'}")
```

## Prerequisites

All scripts require:

1. **PlatformIO**: `pip install platformio`
2. **pyserial**: `pip install pyserial` (auto-installed if missing)
3. **Python 3.7+**

Verify installation:
```bash
which pio
python3 -c "import serial; print('OK')"
```

## Serial Port Configuration

Scripts use these ports by default:
- **Reset/Upload**: `/dev/cu.usbmodem53140032081`
- **Monitor**: `/dev/cu.usbmodem53140032081`

Check available ports:
```bash
ls /dev/cu.usbmodem*
```

If your port is different, use `--port` option:
```bash
python3 scripts/esp32_workflow.py --port /dev/cu.usbmodem101
```

## Troubleshooting

### "PlatformIO not found"
```bash
pip install platformio
# Or: pip3 install platformio
```

### "pyserial not found"
```bash
pip install pyserial
```

### "Port not found"
```bash
# Check available ports
ls /dev/cu.*

# Update platformio.ini with correct port
# Or use --port option
```

### "Permission denied" (Linux)
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

### "FTP connection timeout"
1. Board may not be fully booted
2. Wrong IP address
3. WiFi connection failed

Check serial output:
```bash
python3 scripts/esp32_workflow.py --monitor-only
```

Look for:
```
[WiFi] Connected, IP: 192.168.0.66
[Main] FTP ready at ftp://192.168.0.66
```

## Workflow Steps

All full-workflow scripts perform these steps:

1. **Check Prerequisites** - Verify PlatformIO, pyserial, paths
2. **Clean** - Remove old build files (recommended)
3. **Build** - Compile firmware
4. **Upload** - Flash to ESP32-S3
5. **Reset** - Trigger board reset via serial DTR/RTS
6. **Monitor** - Watch serial output for boot messages
7. **Detect IP** - Extract IP from serial output
8. **Test** - Run FTP connection and transfer tests
9. **Report** - Display results summary

## Environment Variables

Scripts recognize these environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `FTP_IP` | FTP server IP | 192.168.0.66 |
| `FTP_USER` | FTP username | esp32 |
| `FTP_PASS` | FTP password | esp32pass |
| `SERIAL_PORT` | Port for reset | /dev/cu.usbmodem53140032081 |
| `MONITOR_PORT` | Port for monitoring | /dev/cu.usbmodem53140032081 |

Example:
```bash
export FTP_IP=192.168.1.100
python3 scripts/esp32_workflow.py
```

## Comparison

| Feature | `esp32_workflow.py` | `test_ftp.py` | `workflow.sh` |
|---------|---------------------|---------------|---------------|
| Build/Upload | ✅ | ❌ | ✅ |
| Board Reset | ✅ | ❌ | ✅ |
| Serial Monitor | ✅ | ❌ | ✅ |
| IP Auto-detect | ✅ | ❌ | ✅ |
| FTP Tests | ✅ | ✅ | ✅ |
| Error Recovery | ✅ | ❌ | ⚠️ |
| Cross-platform | ✅ | ✅ | ❌ (Unix only) |
| Speed | Medium | Fast | Medium |

## Quick Start

**First time:**
```bash
cd esp32-s3-usb-msc
python3 scripts/esp32_workflow.py
```

**Subsequent runs (faster):**
```bash
python3 scripts/esp32_workflow.py --skip-build
```

**Just test (after manual reset):**
```bash
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

**Monitor serial:**
```bash
python3 scripts/esp32_workflow.py --monitor-only
# Press Ctrl+C to exit
```

## See Also

- **../WORKFLOW_GUIDE.md** - Complete workflow documentation
- **../QUICK_REFERENCE.md** - Quick command reference
- **../FTP_STATUS_REPORT.md** - Bug analysis and workarounds
