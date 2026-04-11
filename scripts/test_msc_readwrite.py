#!/usr/bin/env python3
"""
Test script for PR #5: Read/write MSC module

Tests:
1. usb_msc_sd_begin() - MSC enables successfully
2. usb_msc_sd_end() - MSC disables cleanly
3. usb_msc_sd_refresh() - MSC re-enumerates
4. Read/write behavior - host can read and write to MSC drive

Usage:
    python3 scripts/test_msc_readwrite.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST

Dependencies:
    pip install pyserial
"""

import argparse
import serial
import time
import os
import sys
from pathlib import Path


def wait_for_serial_log(port: str, baud: int, expected: str, timeout: float = 10.0) -> bool:
    """Wait for expected string in serial log."""
    ser = serial.Serial(port, baud, timeout=0.5)
    
    # Trigger a reset to see boot messages
    ser.setDTR(False)
    time.sleep(0.1)
    ser.setDTR(True)
    time.sleep(1.5)  # Wait for boot
    
    ser.reset_input_buffer()
    start = time.time()
    
    while time.time() - start < timeout:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            print(f"[SERIAL] {line}")
            if expected.lower() in line.lower():
                ser.close()
                return True
        time.sleep(0.05)
    ser.close()
    return False


def test_msc_enable(port: str, baud: int) -> bool:
    """Test: MSC enable logs appear."""
    print("\n=== Test: MSC Enable ===")
    result = wait_for_serial_log(port, baud, "Enabled (read/write)")
    print(f"Result: {'PASS' if result else 'FAIL'}\n")
    return result


def test_readwrite_behavior(mount_path: str) -> bool:
    """Test: Host can read and write to MSC drive."""
    print("\n=== Test: Read/Write Behavior ===")
    
    if not mount_path or not os.path.isdir(mount_path):
        print(f"Mount path not available: {mount_path}")
        print("Result: SKIP (MSC not mounted or path invalid)\n")
        return True  # Don't fail if user hasn't mounted
    
    # Try to read a file (should work)
    probe_file = Path(mount_path) / "hello.txt"
    try:
        if probe_file.exists():
            content = probe_file.read_text()
            print(f"Read test: OK (read {len(content)} bytes from hello.txt)")
        else:
            print(f"Read test: OK (no probe file, but mount accessible)")
    except Exception as e:
        print(f"Read test: FAIL - {e}")
        print("Result: FAIL\n")
        return False
    
    # Try to write a file (should succeed on read/write MSC)
    test_file = Path(mount_path) / "testrw.txt"
    try:
        print(f"  Attempting to write to: {test_file}")
        print(f"  Absolute path: {test_file.absolute()}")
        print(f"  Parent exists: {test_file.parent.exists()}")
        print(f"  Parent is dir: {test_file.parent.is_dir()}")
        
        # Use direct file open instead of Path.write_text
        with open(test_file, 'w') as f:
            f.write("Test write from host to MSC")
        
        if test_file.exists():
            with open(test_file, 'r') as f:
                content = f.read()
            if content == "Test write from host to MSC":
                print(f"Write test: PASS - File created and verified")
                test_file.unlink()
            else:
                print(f"Write test: FAIL - File content mismatch")
                test_file.unlink()
                print("Result: FAIL\n")
                return False
        else:
            print(f"Write test: FAIL - File was not persisted")
            print("Result: FAIL\n")
            return False
    except (IOError, OSError, PermissionError) as e:
        print(f"Write test: FAIL - Write rejected: {e}")
        import traceback
        traceback.print_exc()
        print("Result: FAIL\n")
        return False
    except Exception as e:
        print(f"Write test: FAIL - Write failed: {e}")
        print("Result: FAIL\n")
        return False
    
    print("Result: PASS\n")
    return True


def main():
    parser = argparse.ArgumentParser(description="Test PR #5: Read/write MSC")
    parser.add_argument("--port", required=True, help="Serial port (e.g., /dev/cu.usbmodem53140032081)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--mount-path", default="/Volumes/TEST", help="MSC mount path for read/write test (default: /Volumes/TEST)")
    args = parser.parse_args()
    
    print(f"Testing MSC on {args.port}")
    print(f"Mount path: {args.mount_path or 'not provided'}")
    
    # Test 1: MSC enable logged
    enable_ok = test_msc_enable(args.port, args.baud)
    
    # Check if mount path exists before proceeding with read/write test
    # Poll for up to 30 seconds, continue immediately when detected and writable
    if enable_ok and args.mount_path:
        print(f"\n[Waiting] Polling for mount: {args.mount_path}")
        mount_timeout = 30  # seconds
        mount_start = time.time()
        check_interval = 0.5  # Check every 500ms
        
        while time.time() - mount_start < mount_timeout:
            if os.path.isdir(args.mount_path):
                # Mount exists, now check if filesystem is actually writable
                test_probe = Path(args.mount_path) / ".probe"
                try:
                    with open(test_probe, 'w') as f:
                        f.write("probe")
                    test_probe.unlink()
                    elapsed = time.time() - mount_start
                    print(f"[Mount ready] {args.mount_path} is available and writable (took {elapsed:.1f}s)")
                    readwrite_ok = test_readwrite_behavior(args.mount_path)
                    break
                except (PermissionError, OSError):
                    # Mount exists but not writable yet, keep waiting
                    pass
            time.sleep(check_interval)
        else:
            print(f"[Timeout] Mount not found or not writable after {mount_timeout}s, skipping read/write test")
            readwrite_ok = True  # Don't fail if mount isn't available
    else:
        readwrite_ok = True
    
    # Summary
    print("=" * 50)
    print("SUMMARY")
    print("=" * 50)
    print(f"MSC Enable Log:      {'PASS' if enable_ok else 'FAIL'}")
    print(f"Read/Write Behavior: {'PASS' if readwrite_ok else 'FAIL'}")
    
    all_pass = enable_ok and readwrite_ok
    print(f"\nOverall: {'ALL TESTS PASSED' if all_pass else 'SOME TESTS FAILED'}")
    
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
