# ESP32-S3 Project Documentation Index

Complete documentation for the ESP32-S3 SD/MSC/FTP project with workflow automation.

---

## 📚 Documentation Files

### For Quick Start
| Document | Purpose | Read Time |
|----------|---------|-----------|
| **QUICK_REFERENCE.md** | Command cheat sheet, one-liners | 5 min |
| **scripts/esp32_workflow.py** | Automated workflow script | Use directly |

### For Understanding the Project
| Document | Purpose | Read Time |
|----------|---------|-----------|
| **WORKFLOW_GUIDE.md** | Complete workflow documentation | 15 min |
| **FTP_STATUS_REPORT.md** | Bug analysis and library patches | 20 min |

### For Using Scripts
| Document | Purpose | Read Time |
|----------|---------|-----------|
| **scripts/README.md** | Script usage guide | 10 min |

---

## 🚀 Quick Start (3 Steps)

### Step 1: Run Automated Workflow
```bash
cd esp32-s3-usb-msc

# Full workflow (build → upload → test)
python3 scripts/esp32_workflow.py

# Or faster (skip build)
python3 scripts/esp32_workflow.py --skip-build
```

### Step 2: Check Results
Look for:
- ✅ Build: SUCCESS
- ✅ Upload: SUCCESS  
- ✅ Boot: IP detected (192.168.0.66)
- ⚠️ FTP Upload: FAIL (expected, library bug)

### Step 3: Use USB for File Transfer
Since FTP uploads are broken:
1. Connect ESP32 to computer via USB
2. SD card appears as USB drive
3. Copy files directly

---

## 📖 Documentation Details

### 1. QUICK_REFERENCE.md
**What it is:** One-page reference card with all commands  
**Use when:** You need a quick command reminder  
**Contains:**
- One-command workflow
- PlatformIO commands
- Serial port info
- FTP connection details
- Troubleshooting quick fixes
- Useful bash aliases

**Access:**
```bash
cat QUICK_REFERENCE.md
```

### 2. WORKFLOW_GUIDE.md
**What it is:** Complete workflow documentation  
**Use when:** You need detailed instructions  
**Contains:**
- Step-by-step manual workflow
- Automated workflow script (workflow.sh)
- Serial monitoring techniques
- WiFi configuration
- Troubleshooting deep-dive
- Project structure reference

**Access:**
```bash
cat WORKFLOW_GUIDE.md
```

### 3. FTP_STATUS_REPORT.md
**What it is:** Technical status and bug analysis  
**Use when:** You need to understand why FTP uploads fail  
**Contains:**
- All patches applied to SimpleFTPServer library
- Root cause analysis of FTP upload bug
- Why direct SD writes work but FTP doesn't
- 5 alternative workarounds
- Current status of all features

**Access:**
```bash
cat FTP_STATUS_REPORT.md
```

### 4. scripts/README.md
**What it is:** Script usage and API guide  
**Use when:** You want to customize or extend scripts  
**Contains:**
- Script comparison table
- Python API usage
- Environment variables
- Prerequisites
- Troubleshooting

**Access:**
```bash
cat scripts/README.md
```

---

## 🔧 Main Scripts

### Primary Script: `scripts/esp32_workflow.py`
**Purpose:** Full automated workflow  
**Language:** Python 3  
**Features:**
- Build and upload firmware
- Reset board via serial
- Monitor boot process
- Detect IP address
- Run FTP tests
- Color-coded output

**Usage:**
```bash
# Full workflow
python3 scripts/esp32_workflow.py

# Options
python3 scripts/esp32_workflow.py --help

# Skip build for faster testing
python3 scripts/esp32_workflow.py --skip-build

# Just monitor serial
python3 scripts/esp32_workflow.py --monitor-only
```

### Legacy Script: `scripts/test_ftp.py`
**Purpose:** FTP testing only  
**Language:** Python 3  
**Use when:** Board is already running, just need to test FTP

**Usage:**
```bash
python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass
```

### Bash Script: `workflow.sh`
**Purpose:** Bash version of full workflow  
**Language:** Bash  
**Use when:** You prefer bash or need Unix-only features

**Usage:**
```bash
./workflow.sh
./workflow.sh --skip-build
```

---

## 🎯 Common Tasks

### Task: Upload New Code Changes
```bash
# Full rebuild and test
python3 scripts/esp32_workflow.py
```

### Task: Test Without Rebuilding
```bash
# Just reset and test
python3 scripts/esp32_workflow.py --skip-build
```

### Task: Check Serial Output
```bash
# Real-time monitoring
python3 scripts/esp32_workflow.py --monitor-only
# Press Ctrl+C to stop
```

### Task: Find Board IP Address
```bash
# IP is auto-detected during workflow, or:
grep "Connected, IP" /path/to/serial/log
```

### Task: Connect via FTP (Command Line)
```bash
ftp 192.168.0.66
# Login: esp32 / esp32pass
# Commands: ls, cd /sd, get file.txt, put file.txt, quit
```

---

## 📊 Feature Status

| Feature | Status | Notes |
|---------|--------|-------|
| SD Card Read | ✅ Working | USB MSC, FTP download |
| SD Card Write | ✅ Working | USB MSC only |
| WiFi Connection | ✅ Working | Auto-connects on boot |
| FTP Server Start | ✅ Working | Accepts connections |
| FTP Auth | ✅ Working | Login successful |
| FTP Directory List | ✅ Working | Can list files |
| FTP Download | ✅ Working | GET files works |
| **FTP Upload** | ❌ **Broken** | "552 Probably insufficient storage" |

**Workaround:** Use USB MSC for file transfers

---

## 🔌 Hardware Setup

### Required
- ESP32-S3-DevKitC-1
- MicroSD card (SDHC/SDXC, tested with 16GB)
- USB-C cable (data + power)

### Pin Connections (SPI)
| SD Card | ESP32-S3 | Function |
|---------|----------|----------|
| SCK | GPIO 18 | Clock |
| MISO | GPIO 16 | Data In |
| MOSI | GPIO 17 | Data Out |
| CS | GPIO 15 | Chip Select |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### Software
- PlatformIO Core
- Python 3.7+
- pyserial library

---

## 🐛 Known Issues

### Issue #1: FTP Upload Fails
**Symptom:** "552 Probably insufficient storage space"  
**Cause:** SimpleFTPServer library v3.0.2 bug with Arduino SD on ESP32-S3  
**Workaround:** Use USB MSC instead  
**Status:** Cannot be fixed without library changes

### Issue #2: IP Address May Change
**Symptom:** FTP connection times out  
**Cause:** DHCP assigns different IP  
**Workaround:** Check serial output for current IP  
**Automation:** `esp32_workflow.py` auto-detects IP

### Issue #3: Serial Port Name Changes
**Symptom:** "Port not found"  
**Cause:** macOS/Linux assign different /dev names  
**Workaround:** Check `ls /dev/cu.usbmodem*` and update  
**Automation:** Scripts accept `--port` argument

---

## 📝 Project File Structure

```
esp32-s3-usb-msc/
├── 📄 DOCUMENTATION_INDEX.md  ← You are here
├── 📄 QUICK_REFERENCE.md      ← Command cheat sheet
├── 📄 WORKFLOW_GUIDE.md       ← Complete workflow
├── 📄 FTP_STATUS_REPORT.md    ← Technical status
├── 🔧 workflow.sh             ← Bash automation script
│
├── 📁 include/
│   └── config.h               ← WiFi credentials
│
├── 📁 lib/
│   └── SimpleFTPServer/       ← Patched FTP library
│       └── FtpServer.h        ← (patches applied)
│
├── 📁 src/
│   ├── main.cpp               ← Main application
│   ├── sd_card.cpp/h          ← SD card interface
│   ├── usb_msc_sd.cpp/h       ← USB MSC implementation
│   ├── ftp_service.cpp/h      ← FTP server
│   ├── settings_store.cpp/h   ← NVS settings
│   └── app_state.cpp/h        ← State management
│
├── 📁 scripts/
│   ├── 📄 README.md           ← Script documentation
│   ├── 🔧 esp32_workflow.py   ← ⭐ Python automation
│   ├── 🔧 test_ftp.py         ← FTP testing
│   └── 📁 __pycache__/        ← (Python cache)
│
├── ⚙️  platformio.ini          ← Build configuration
├── 🔌 partitions_msc.csv      ← Flash partitions
└── 📁 .pio/                   ← (PlatformIO build files)
```

---

## 🎓 Learning Path

### For First-Time Users
1. Read **QUICK_REFERENCE.md** (5 min)
2. Run: `python3 scripts/esp32_workflow.py` (30 sec)
3. Check results
4. Connect ESP32 to computer via USB
5. Copy files to SD card via USB drive

### For Understanding Issues
1. Read **FTP_STATUS_REPORT.md** (20 min)
2. Understand why FTP uploads fail
3. Learn about library patches
4. See workarounds

### For Customization
1. Read **WORKFLOW_GUIDE.md** (15 min)
2. Read **scripts/README.md** (10 min)
3. Modify `include/config.h` for your WiFi
4. Customize scripts as needed

---

## 🔗 Cross-References

| From Document | Links To |
|---------------|----------|
| DOCUMENTATION_INDEX.md | All other docs |
| QUICK_REFERENCE.md | WORKFLOW_GUIDE.md, FTP_STATUS_REPORT.md |
| WORKFLOW_GUIDE.md | scripts/README.md, FTP_STATUS_REPORT.md |
| FTP_STATUS_REPORT.md | WORKFLOW_GUIDE.md |
| scripts/README.md | esp32_workflow.py, test_ftp.py |

---

## 🆘 Getting Help

### Check These First
1. **QUICK_REFERENCE.md** → Troubleshooting section
2. **WORKFLOW_GUIDE.md** → Detailed troubleshooting
3. **FTP_STATUS_REPORT.md** → Known issues

### Useful Commands
```bash
# Check ports
ls /dev/cu.usbmodem*

# Check PlatformIO
which pio
pio --version

# Check Python
python3 --version
python3 -c "import serial; print('pyserial OK')"

# Monitor serial for errors
python3 scripts/esp32_workflow.py --monitor-only
```

### Serial Output Check
Look for these messages in boot output:
```
[SD] Initialized successfully          ← SD card OK
[WiFi] Connected, IP: 192.168.0.66     ← WiFi OK
[FTP] Server started                   ← FTP OK
```

---

## 💡 Tips for Future Sessions

### Setup Once, Use Many Times
Add to `~/.bashrc` or `~/.zshrc`:
```bash
alias esp32='cd ~/projects/esp32-s3-usb-msc'
alias esp32test='python3 ~/projects/esp32-s3-usb-msc/scripts/esp32_workflow.py --skip-build'
alias esp32monitor='pio device monitor --port /dev/cu.usbmodem53140032081 --baud 115200'
```

### Bookmark This Index
This file (`DOCUMENTATION_INDEX.md`) is the master reference.

### Keep Serial Monitor Open
While developing, keep one terminal with:
```bash
python3 scripts/esp32_workflow.py --monitor-only
```

### Use USB MSC for Files
Don't fight the FTP bug - USB MSC is faster and more reliable anyway.

---

## 📅 Document Versions

| Document | Last Updated | Version |
|----------|--------------|---------|
| DOCUMENTATION_INDEX.md | Today | 1.0 |
| QUICK_REFERENCE.md | Today | 1.0 |
| WORKFLOW_GUIDE.md | Today | 1.0 |
| FTP_STATUS_REPORT.md | Today | 1.0 |
| scripts/README.md | Today | 1.0 |
| esp32_workflow.py | Today | 1.0 |

---

**Project:** ESP32-S3 SD/MSC/FTP  
**Status:** Functional (USB MSC), FTP uploads known broken  
**Documentation:** Complete with automation scripts

**Quick Command:**
```bash
cd esp32-s3-usb-msc && python3 scripts/esp32_workflow.py
```
