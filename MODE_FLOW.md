# ESP32-SD Mode Flow Guide

## Boot Behavior

**Default: ALWAYS starts in MSC mode**
- SD card immediately available as USB drive
- Device A can read files right away
- No WiFi or HTTP started

## Mode Switching

### MSC Mode → HTTP Mode (Serial Command)

1. **Connect ESP32 via USB**
2. **Open serial monitor** (115200 baud):
   ```bash
   screen /dev/cu.usbmodem53140032081 115200
   # OR
   pio device monitor --port /dev/cu.usbmodem53140032081 --baud 115200
   ```
3. **Type 'h' and press Enter**
4. **ESP32 will:**
   - Stop MSC
   - Connect WiFi
   - Start HTTP server
   - Show: `HTTP MODE ACTIVE`
5. **Access web interface:**
   - Open browser to http://192.168.0.66/
   - Upload files via web interface
   
### HTTP Mode → MSC Mode (Web Interface)

1. **While in HTTP mode**, open browser
2. **Click "Switch to MSC Mode" button**
3. **Confirm the dialog**
4. **ESP32 will:**
   - Stop HTTP server
   - Disconnect WiFi
   - Restart MSC
   - Show: `MSC Mode Ready`
5. **Unplug ESP32, give to Device A**

### Force Reset to Default

If you get stuck in HTTP mode:

1. **Open serial monitor**
2. **Type 'r' and press Enter**
3. **Restart ESP32**
4. **Will boot in MSC mode (default)**

## State Persistence

| Mode | Saved to Flash? | Survives Reboot? |
|------|----------------|------------------|
| MSC | No (false) | No → Defaults to MSC |
| HTTP | Yes (true) | Yes until reset |

**Important**: Even if you save HTTP mode, the ESP32 defaults to MSC mode on boot for safety.

## Serial Commands

| Command | Action |
|---------|--------|
| `h` | Switch to HTTP mode |
| `r` | Reset all settings to defaults |
| Any other | Show help |

## Complete Workflow Example

```
1. Power on ESP32
   ↓
   MSC starts automatically (default)
   ↓
2. Device A reads files (if needed)
   ↓
3. Connect ESP32 to computer via USB
   ↓
4. Open serial monitor, type 'h'
   ↓
5. HTTP server starts
   ↓
6. Upload new files via browser
   ↓
7. Click "Switch to MSC Mode"
   ↓
8. MSC restarts
   ↓
9. Give ESP32 to Device A to read new files
```

## Web Interface

When in HTTP mode:

```
http://192.168.0.66/

Features:
- File upload form
- List files on SD card
- Delete files
- Switch to MSC Mode button
```

## Troubleshooting

### Can't switch to HTTP mode
- Check serial connection
- Ensure you send 'h' (lowercase)
- Check WiFi credentials in config.h

### Can't switch back to MSC
- Make sure to click the button
- After clicking, wait 2-3 seconds
- Check serial output for confirmation

### MSC not mounting
- Check USB cable supports data
- Try different USB port
- Check SD card is inserted

### Files not persisting
- Ensure proper eject from computer
- Don't unplug during write
- Check SD card isn't write-protected
