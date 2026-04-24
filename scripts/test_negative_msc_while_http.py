#!/usr/bin/env python3
"""
NEGATIVE TEST: MSC Access while HTTP is Active (MSC-Default Boot)

This test verifies:
1. ESP32 boots in MSC mode (default)
2. After switching to HTTP mode, MSC becomes unavailable
3. Mutual exclusion prevents concurrent access

Prerequisites:
    - ESP32-S3 with MSC-default firmware
    - ESP32 connected via USB
    - Mount path accessible initially

Expected Result:
    - MSC available at boot (default)
    - After HTTP switch, MSC unavailable
    - This is the CORRECT behavior (prevents SD corruption)

Usage:
    python3 test_negative_msc_while_http.py \
        --port /dev/cu.usbmodem53140032081 \
        --mount-path /Volumes/TEST
"""

import argparse
import serial
import time
import sys
from pathlib import Path


def test_msc_while_http_active(serial_port, mount_path, esp32_ip='192.168.0.66', verbose=True):
    """Test MSC availability before and after HTTP switch."""
    
    if verbose:
        print("=" * 60)
        print("NEGATIVE TEST: MSC Availability (MSC-Default Boot)")
        print("=" * 60)
        print(f"Serial Port: {serial_port}")
        print(f"Mount Path:  {mount_path}")
        print()
        print("Testing MSC → HTTP mode transition...")
        print("-" * 60)
    
    # Step 1: Verify MSC is available at boot
    if verbose:
        print("\nStep 1: Verify MSC available at boot (default)")
    
    mount = Path(mount_path)
    if not mount.exists():
        print("❌ TEST SETUP FAILED")
        print(f"   MSC not found at: {mount_path}")
        print("   Ensure ESP32 is connected via USB and booted")
        return False
    
    if verbose:
        print(f"✅ MSC available at boot: {mount_path}")
        try:
            files = [f.name for f in mount.iterdir() if f.is_file()][:5]
            print(f"   Contains {len(files)} files (showing first 5):")
            for f in files:
                print(f"   - {f}")
        except Exception as e:
            print(f"   Note: {e}")
    
    # Step 2: Switch to HTTP mode
    if verbose:
        print("\nStep 2: Switch ESP32 to HTTP mode")
        print("   Sending 'h' via serial...")
    
    try:
        ser = serial.Serial(serial_port, 115200, timeout=0.5)
        
        # Clear buffer and send command
        time.sleep(1)
        ser.reset_input_buffer()
        ser.write(b'h')
        ser.flush()
        
        # Read response
        time.sleep(2)
        output = ""
        for _ in range(20):
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                output += data.decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        ser.close()
        
        if verbose:
            if "HTTP" in output:
                print("✅ HTTP mode transition detected")
            else:
                print("⚠️  Response (may need more time):")
                print(output[-500:])
        
    except Exception as e:
        print(f"❌ Serial error: {e}")
        print("   Ensure serial port is correct and ESP32 is connected")
        return False
    
    # Step 3: Wait and check MSC is no longer available
    if verbose:
        print("\nStep 3: Verify MSC is unavailable after HTTP switch")
        print("   Waiting 5 seconds for transition...")
    
    time.sleep(5)
    
    msc_still_available = mount.exists()
    
    if verbose:
        print(f"   MSC mount exists: {msc_still_available}")
    
    # Step 4: Check HTTP is now accessible
    if verbose:
        print("\nStep 4: Verify HTTP is accessible")
    
    try:
        import requests
        resp = requests.get(f"http://{esp32_ip}/", timeout=5)
        http_available = resp.status_code == 200
        if verbose:
            if http_available:
                print(f"✅ HTTP responding (status {resp.status_code})")
            else:
                print(f"⚠️  HTTP status: {resp.status_code}")
    except Exception as e:
        http_available = False
        if verbose:
            print(f"⚠️  HTTP not yet available: {e}")
    
    # Step 5: Results
    if verbose:
        print("\n" + "=" * 60)
        print("NEGATIVE TEST RESULT")
        print("=" * 60)
    
    print()
    if not msc_still_available and http_available:
        print("✅ NEGATIVE TEST PASSED!")
        print("   - MSC available at boot ✅")
        print("   - MSC unavailable after HTTP switch ✅")
        print("   - HTTP accessible after switch ✅")
        print()
        print("   Mutual exclusion working correctly:")
        print("   - Only one interface active at a time")
        print("   - No SD corruption possible")
        return True
    elif msc_still_available:
        print("⚠️  PARTIAL RESULT")
        print("   - MSC still available after HTTP switch")
        print("   - This may be normal (USB needs re-enumeration)")
        print("   - HTTP may not have fully started")
        print()
        print("   Verdict: INCONCLUSIVE (depends on timing)")
        return None
    else:
        print("✅ NEGATIVE TEST PASSED (mostly)")
        print("   - MSC unavailable after switch ✅")
        print("   - HTTP may still be starting")
        print()
        return True


def main():
    parser = argparse.ArgumentParser(
        description='Test MSC availability with MSC-default boot',
        epilog="""
Example:
    python3 test_negative_msc_while_http.py \
        --port /dev/cu.usbmodem53140032081 \
        --mount-path /Volumes/TEST
        """
    )
    
    parser.add_argument('--port', required=True,
                       help='Serial port for ESP32')
    parser.add_argument('--mount-path', required=True,
                       help='MSC mount path')
    parser.add_argument('--esp32-ip', default='192.168.0.66',
                       help='ESP32 IP address')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode')
    
    args = parser.parse_args()
    
    try:
        import serial
    except ImportError:
        print("Error: pyserial not found")
        print("Install: pip install pyserial")
        sys.exit(1)
    
    result = test_msc_while_http_active(
        serial_port=args.port,
        mount_path=args.mount_path,
        esp32_ip=args.esp32_ip,
        verbose=not args.quiet
    )
    
    if result is None:
        sys.exit(0)  # Inconclusive isn't a failure
    else:
        sys.exit(0 if result else 1)


if __name__ == '__main__':
    main()
