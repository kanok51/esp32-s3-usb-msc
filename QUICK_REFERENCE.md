# ESP32-S3 Quick Reference Card

## 🚀 One-Command Workflow
```bash
cd esp32-s3-usb-msc && ./workflow.sh
```

## 📝 Manual Steps

### 1. Build & Upload
```bash
cd esp32-s3-usb-msc
pio run --target upload
```
⏱️ ~25-35 seconds

### 2. Reset Board
- Press **RST** button on ESP32-S3
- OR: Run Python reset script (see WORKFLOW_GUIDE.md)

### 3. Wait for Boot
⏱️ ~10-15 seconds

### 4. Run Tests
```bash
sleep 10 && python3 scripts/test_ftp.py \
    --host 192.168.0.66 \
    --user esp32 \
    --password esp32pass
```

## 🔧 Common Commands

### PlatformIO
| Command | Description |
|---------|-------------|
| `pio run` | Build only |
| `pio run --target clean` | Clean build files |
| `pio run --target upload` | Build & upload |
| `pio device monitor` | Serial monitor |
| `pio device monitor --baud 115200` | Monitor at 115200 |

### Serial Port
| Port | Usage |
|------|-------|
| `/dev/cu.usbmodem101` | Upload/Reset/Monitor |
| `/dev/cu.usbmodem53140032081` | Monitor (configured) |

Check ports: `ls /dev/cu.usbmodem*`

### Serial Output (Python)
```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbmodem53140032081', 115200, timeout=0.1)
for _ in range(100):
    data = ser.read(1024)
    if data: print(data.decode('utf-8', errors='ignore'), end='')
    time.sleep(0.05)
"
```

## 🌐 Network

### FTP Server
- **IP:** 192.168.0.66 (usually)
- **User:** esp32
- **Pass:** esp32pass
- **Port:** 21

### Test FTP Connection
```bash
ftp 192.168.0.66
# Login: esp32 / esp32pass
# Commands: ls, cd /sd, put file.txt, get file.txt, quit
```

### Find IP (if changed)
```bash
# Check serial output for "WiFi Connected, IP:"
# Or scan network:
nmap -sn 192.168.0.0/24 | grep -i esp
```

## 🧪 Testing

### Run All Tests
```bash
python3 scripts/test_ftp.py \
    --host 192.168.0.66 \
    --user esp32 \
    --password esp32pass
```

### Quick Connection Test
```bash
python3 -c "
import ftplib
ftp = ftplib.FTP('192.168.0.66', timeout=10)
ftp.login('esp32', 'esp32pass')
print('✅ Connected!')
ftp.quit()
"
```

## ⚠️ Known Issues

| Issue | Status | Workaround |
|-------|--------|------------|
| FTP uploads fail | 🔴 Known bug | Use USB MSC instead |
| "552 Probably insufficient storage" | 🔴 Library bug | Use USB cable for transfers |
| FTP downloads work | ✅ Working | Download via FTP OK |
| Directory listing works | ✅ Working | List files via FTP OK |

## 📁 Project Structure

```
esp32-s3-usb-msc/
├── include/
│   └── config.h          # WiFi credentials
├── lib/
│   └── SimpleFTPServer/  # Patched library
├── src/
│   ├── main.cpp          # Main app
│   ├── sd_card.cpp       # SD interface
│   ├── usb_msc_sd.cpp    # USB MSC
│   └── ftp_service.cpp   # FTP server
├── scripts/
│   └── test_ftp.py      # Test script
├── platformio.ini        # Build config
├── workflow.sh          # Automated workflow ⭐
├── WORKFLOW_GUIDE.md    # Full documentation
└── QUICK_REFERENCE.md   # This file
```

## 🔌 Hardware

### Pin Connections (SPI)
| SD Card | ESP32-S3 | Pin # |
|---------|----------|-------|
| SCK     | GPIO 18  | 18    |
| MISO    | GPIO 16  | 16    |
| MOSI    | GPIO 17  | 17    |
| CS      | GPIO 15  | 15    |
| VCC     | 3.3V     | -     |
| GND     | GND      | -     |

### Boot Sequence LED Indicators
- No serial output → Check USB connection
- "SD init failed" → Check SD card/pins
- "WiFi timeout" → Check credentials in config.h

## 🆘 Troubleshooting

### No Serial Output
```bash
# Check port
ls /dev/cu.usb*

# Try different baud
pio device monitor --baud 115200
```

### Upload Fails
```bash
# Check port exists
ls /dev/cu.usbmodem*

# Specify port manually
pio run --target upload --upload-port /dev/cu.usbmodem101
```

### Wrong IP Address
1. Check serial output for actual IP
2. Update test command with correct IP
3. Or use: `ping 192.168.0.66` to check if reachable

### FTP Test Times Out
```bash
# Reset board first
# Wait longer
sleep 20
# Then run test
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

## 💾 File Transfer (Recommended)

Since FTP uploads are broken, use **USB MSC**:

1. Connect ESP32 to computer via USB
2. SD card appears as USB drive
3. Copy files directly in file manager
4. Eject safely before disconnecting

This is faster and more reliable than FTP.

## 📊 Expected Test Results

```
Testing FTP on 192.168.0.66
Credentials: esp32 / *********

=== Test: FTP Connection ===
Connecting to 192.168.0.66...
Connected successfully
Result: PASS ✅

=== Test: FTP Upload/Download ===
FTP error: 552 Probably insufficient storage space
Result: FAIL ❌ (expected - library bug)
```

## 🔗 Useful Aliases

Add to `~/.bashrc` or `~/.zshrc`:

```bash
alias esp32build='cd ~/projects/esp32-s3-usb-msc && pio run --target upload'
alias esp32monitor='pio device monitor --port /dev/cu.usbmodem53140032081 --baud 115200'
alias esp32test='cd ~/projects/esp32-s3-usb-msc && python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass'
alias esp32reset='python3 -c "import serial; ser=serial.Serial(\"/dev/cu.usbmodem53140032081\",115200); ser.dtr=True; ser.rts=False; ser.dtr=False; ser.rts=True; ser.dtr=False; ser.rts=False; ser.close()"'
```

## 📚 Full Documentation

- **WORKFLOW_GUIDE.md** - Complete workflow documentation
- **FTP_STATUS_REPORT.md** - Detailed bug analysis and fixes
- **README.md** - Project overview (create if needed)

---

**Project:** ESP32-S3 SD/MSC/FTP  
**Status:** USB MSC working, FTP uploads broken  
**Workaround:** Use USB cable for file transfers
