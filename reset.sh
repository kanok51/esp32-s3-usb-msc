#!/bin/bash
# ESP32-S3 Reset Script (Bash version)
# Resets ESP32 via serial and displays IP from boot log

PORT="${1:-/dev/cu.usbmodem53140032081}"
WAITS="${2:-15}"

echo "Resetting ESP32 on $PORT..."
echo ""

# Python script for reset + IP capture
python3 -c "
import serial
import time
import re

port = '$PORT'
wait_time = int('$WAITS')

# Reset
ser = serial.Serial(port, 115200, timeout=0.1)
ser.dtr = True
ser.rts = False
time.sleep(0.1)
ser.dtr = False
ser.rts = True
time.sleep(0.1)
ser.dtr = False
ser.rts = False
time.sleep(0.5)
ser.close()

echo('✅ Reset command sent')
echo('')
echo('Monitoring boot for {} seconds...'.format(wait_time))
echo('-' * 50)

# Monitor boot
ser = serial.Serial(port, 115200, timeout=0.1)
output = ''
start = time.time()

while time.time() - start < wait_time:
    data = ser.read(1024)
    if data:
        text = data.decode('utf-8', errors='ignore')
        output += text
        echo(text, end='', flush=True)

ser.close()

# Parse results
ip_match = re.search(r'Connected, IP:\s*([\d.]+)', output)
mode = None

if 'MSC Mode Ready' in output or 'MSC mode active' in output:
    mode = 'MSC'
elif 'HTTP MODE ACTIVE' in output or 'HTTP mode active' in output:
    mode = 'HTTP'

echo('')
echo('-' * 50)
echo('')
echo('=' * 50)
echo('  BOOT SUMMARY')
echo('=' * 50)
echo('')

if mode:
    echo('  Detected Mode: {}'.format(mode))
else:
    echo('  Mode: Unknown (still booting?)')

if ip_match:
    ip = ip_match.group(1)
    echo('  WiFi IP Address: {}'.format(ip))
    echo('')
    echo('  URL: http://{}/'.format(ip))
    echo('')
    echo('  To upload: python3 scripts/upload_file.py file.txt --ip {}'.format(ip))
    echo('')
    if mode == 'MSC':
        echo('  To switch to HTTP: Send h via serial')
    else:
        echo('  To switch to MSC: Click button in web interface')
elif mode == 'HTTP':
    echo('  IP: WiFi still connecting...')
    echo('')
    echo('  Try again with longer wait time')
else:
    echo('  IP: N/A')
    echo('  WiFi may still be connecting')

echo('')
echo('=' * 50)
echo('')
echo('✅ ESP32 reset complete')
"

echo ""

# Keep monitoring if --monitor flag given
if [ "$3" == "--monitor" ]; then
    echo ""
    echo "Starting continuous monitor (Ctrl+C to exit)..."
    echo "--------------------------------------------------"
    python3 -c "
import serial
import time
import sys

ser = serial.Serial('$PORT', 115200, timeout=0.1)
try:
    while True:
        data = ser.read(1024)
        if data:
            print(data.decode('utf-8', errors='ignore'), end='', flush=True)
        time.sleep(0.01)
except KeyboardInterrupt:
    pass
ser.close()
"
fi
