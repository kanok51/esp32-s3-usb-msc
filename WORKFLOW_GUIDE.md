# ESP32-S3 Upload, Reset & Test Workflow Guide

## Quick Reference

### One-Command Full Workflow
```bash
cd esp32-s3-usb-msc && ./workflow.sh
```

## Manual Step-by-Step Workflow

### Step 1: Build and Upload Firmware
```bash
cd esp32-s3-usb-msc

# Clean build (recommended after code changes)
pio run --target clean

# Build and upload
pio run --target upload
```

**Expected Output:**
```
========================= [SUCCESS] Took ~25-35 seconds =========================
```

### Step 2: Reset the Board
The firmware includes an automatic post-upload reset, but manual reset may be needed:

**Option A: Physical Reset (Recommended)**
- Press the **RST** button on the ESP32-S3 dev board
- Wait 10-15 seconds for boot

**Option B: Serial DTR/RTS Reset**
```bash
# Using Python
python3 -c "
import serial
ser = serial.Serial('/dev/cu.usbmodem53140032081', 115200)
ser.dtr = True
ser.rts = False
ser.dtr = False
ser.rts = True
ser.dtr = False
ser.rts = False
ser.close()
print('Reset sent')
"
```

### Step 3: Verify Boot (Check Serial Output)
```bash
# Read serial output for 15 seconds
python3 -c "
import serial
import time

ser = serial.Serial('/dev/cu.usbmodem53140032081', 115200, timeout=0.1)
time.sleep(1)  # Wait for boot to start

print('Reading serial output...')
start = time.time()
while time.time() - start < 15:
    data = ser.read(1024)
    if data:
        print(data.decode('utf-8', errors='ignore'), end='')
    time.sleep(0.01)
ser.close()
" 2>/dev/null
```

**Expected Boot Output:**
```
========================================
  ESP32-S3 SD / MSC / FTP / Web UI
========================================

[SD] Card type: SDHC/SDXC
[SD] Card size: 15193 MB
[WiFi] Connected, IP: 192.168.0.66
[Main] FTP ready at ftp://192.168.0.66
```

### Step 4: Run FTP Test
```bash
# Wait for boot to complete (WiFi connection takes ~5-10s)
sleep 10

# Run FTP test
cd esp32-s3-usb-msc
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

**Expected Results:**
- FTP Connection: ✅ PASS
- FTP Upload/Download: ❌ FAIL (known library issue)

### Step 5: Manual FTP Test
```bash
# Connect via command-line FTP
ftp 192.168.0.66

# Login
cd /sd
ls
put localfile.txt
get remotefile.txt
quit
```

## Automated Workflow Script

### Full Automation Script
Save as `workflow.sh`:
```bash
#!/bin/bash
# ESP32-S3 Workflow: Build, Upload, Reset, Test

set -e  # Exit on error

PROJECT_DIR="$(dirname "$0")"
cd "$PROJECT_DIR"

echo "========================================"
echo "ESP32-S3 Workflow: Build → Upload → Test"
echo "========================================"
echo ""

# Step 1: Build and Upload
echo "[1/4] Building and uploading firmware..."
pio run --target upload

# Step 2: Wait for post-upload reset
echo ""
echo "[2/4] Waiting for post-upload reset (5s)..."
sleep 5

# Step 3: Wait for boot
echo ""
echo "[3/4] Waiting for board boot (15s)..."
sleep 15

# Step 4: Run tests
echo ""
echo "[4/4] Running FTP tests..."
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass

echo ""
echo "========================================"
echo "Workflow Complete"
echo "========================================"
```

Make executable:
```bash
chmod +x workflow.sh
```

## Serial Port Reference

### Available Ports
| Port | Description | Use Case |
|------|-------------|----------|
| `/dev/cu.usbmodem101` | USB-Serial/JTAG | Upload, Reset, Monitor |
| `/dev/cu.usbmodem53140032081` | USB-Serial/JTAG | Monitor (as configured in platformio.ini) |

### Port Detection
```bash
# List available USB serial ports
ls /dev/cu.usbmodem*

# Or with details
ls -la /dev/cu.usb*
```

## PlatformIO Commands Reference

### Build Commands
```bash
# Build only
pio run

# Clean build
pio run --target clean

# Build and upload
pio run --target upload

# Upload only (if already built)
pio run --target upload --upload-no-build
```

### Monitoring Commands
```bash
# Serial monitor (continuous)
pio device monitor

# Serial monitor with specific port
pio device monitor --port /dev/cu.usbmodem101

# Monitor with custom baud rate
pio device monitor --baud 115200
```

### File System Commands
```bash
# Upload filesystem (if using SPIFFS/LittleFS)
pio run --target uploadfs

# Erase flash
pio run --target erase
```

## Troubleshooting

### Issue: Upload fails with "Port not found"
**Solution:**
```bash
# Check if port exists
ls /dev/cu.usbmodem*

# If port is different, update platformio.ini
# Or specify port manually:
pio run --target upload --upload-port /dev/cu.usbmodem101
```

### Issue: FTP test times out
**Solution:**
```bash
# 1. Reset the board
# 2. Wait longer for WiFi connection
sleep 20

# 3. Check if board is responding
ping -c 3 192.168.0.66

# 4. Try test again
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

### Issue: Serial output garbled
**Solution:**
```bash
# Use correct baud rate
pio device monitor --baud 115200

# Or Python
python3 -c "
import serial
ser = serial.Serial('/dev/cu.usbmodem53140032081', 115200)
while True:
    print(ser.read().decode('utf-8', errors='ignore'), end='')
"
```

### Issue: Build fails
**Solution:**
```bash
# Clean and rebuild
pio run --target clean
pio run

# Check library dependencies
pio lib list

# Update libraries
pio lib update
```

## WiFi Configuration

The WiFi credentials are configured in `include/config.h`:
```cpp
#define APP_WIFI_SSID        "your_wifi_ssid"
#define APP_WIFI_PASSWORD    "your_wifi_password"
```

If the IP address changes:
```bash
# Check serial output for new IP
# Or use network scan
nmap -sn 192.168.0.0/24 | grep -i esp
```

## Test Script Options

### Full Test
```bash
python3 scripts/test_ftp.py \
    --host 192.168.0.66 \
    --user esp32 \
    --password esp32pass
```

### Connection Only Test
```bash
python3 -c "
import ftplib
try:
    ftp = ftplib.FTP('192.168.0.66', timeout=10)
    ftp.login('esp32', 'esp32pass')
    print('✅ FTP Connection: SUCCESS')
    ftp.quit()
except Exception as e:
    print(f'❌ FTP Connection: FAILED - {e}')
"
```

### Manual File Operations
```bash
# List files
python3 -c "
import ftplib
ftp = ftplib.FTP('192.168.0.66')
ftp.login('esp32', 'esp32pass')
ftp.retrlines('LIST')
ftp.quit()
"

# Upload test file
python3 -c "
import ftplib
import io
ftp = ftplib.FTP('192.168.0.66')
ftp.login('esp32', 'esp32pass')
data = io.BytesIO(b'Hello from test')
ftp.storbinary('STOR test.txt', data)
ftp.quit()
print('Upload attempted')
"
```

## Project Structure Reference

```
esp32-s3-usb-msc/
├── include/
│   └── config.h              # WiFi/FTP credentials
├── lib/
│   └── SimpleFTPServer/      # Patched FTP library
│       └── FtpServer.h       # ← Storage fixes applied
├── src/
│   ├── main.cpp              # Main application
│   ├── sd_card.cpp/h         # SD card interface
│   ├── usb_msc_sd.cpp/h      # USB MSC implementation
│   ├── ftp_service.cpp/h     # FTP server
│   └── ...
├── scripts/
│   └── test_ftp.py          # FTP test script
├── platformio.ini           # Build configuration
├── workflow.sh              # ← This workflow script
└── WORKFLOW_GUIDE.md        # ← This guide
```

## Quick Checklist

Before starting workflow:
- [ ] ESP32-S3 connected via USB
- [ ] SD card inserted
- [ ] WiFi network available
- [ ] PlatformIO installed: `which pio`
- [ ] pyserial installed: `pip3 install pyserial`

During workflow:
- [ ] Build successful
- [ ] Upload successful
- [ ] Board reset complete
- [ ] Serial output shows correct IP
- [ ] FTP connection successful

## Notes for Future Sessions

### Library Status
- SimpleFTPServer v3.0.2 has known issues with ESP32-S3 + Arduino SD
- Patches applied for storage type and free space calculation
- FTP uploads fail with "552 Probably insufficient storage space"
- Use USB MSC instead for reliable file transfers

### Known Good Configurations
- Platform: espressif32 @ 6.13.0
- Board: esp32-s3-devkitc-1
- Framework: arduino
- Upload protocol: esptool
- Serial baud: 115200

### Useful Aliases
Add to `~/.bashrc` or `~/.zshrc`:
```bash
alias esp32build='cd ~/projects/esp32-s3-usb-msc && pio run --target upload'
alias esp32monitor='pio device monitor --port /dev/cu.usbmodem53140032081 --baud 115200'
alias esp32test='cd ~/projects/esp32-s3-usb-msc && python3 scripts/test_ftp.py'
```

---

**Last Updated:** $(date)
**Project:** ESP32-S3 SD/MSC/FTP
**Status:** FTP uploads known broken, use USB MSC instead
