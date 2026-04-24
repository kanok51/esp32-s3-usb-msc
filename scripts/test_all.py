#!/usr/bin/env python3
"""
Test Suite Runner for ESP32-S3 MSC-Default + HTTP Dual Interface
Runs all tests with proper context for MSC-first boot behavior.

Usage:
    # Run all tests
    python3 test_all.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST

    # Skip negative tests (faster)
    python3 test_all.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST --skip-negative
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path


# Test definitions for MSC-default firmware
TESTS = {
    'upload_simple': {
        'script': 'upload_file.py',
        'description': 'Simple file upload (requires manual HTTP switch first)',
        'needs': ['manual_mode_switch'],
    },
    'workflow': {
        'script': 'test_workflow.py',
        'description': 'Full MSC → HTTP → Upload → MSC workflow',
        'needs': ['serial_port', 'mount_path'],
    },
    'http_while_msc': {
        'script': 'test_negative_http_while_msc.py',
        'description': 'Negative: HTTP blocked while MSC active (default)',
        'needs': ['mount_path'],
    },
    'msc_while_http': {
        'script': 'test_negative_msc_while_http.py',
        'description': 'Negative: MSC unavailable after HTTP switch',
        'needs': ['serial_port', 'mount_path'],
    },
}


def run_test(script_path, args, test_name, verbose=True):
    """Run a test script and return result."""
    cmd = [sys.executable, str(script_path)]
    
    # Add appropriate arguments based on test
    if test_name == 'upload_simple':
        # upload_file.py needs different args
        cmd = [sys.executable, str(script_path), '/tmp/test_upload.txt']
        if args.esp32_ip:
            cmd.extend(['--ip', args.esp32_ip])
    else:
        # Other tests use --port and --mount-path
        if args.port and test_name in ['workflow', 'msc_while_http']:
            cmd.extend(['--port', args.port])
        
        if args.mount_path:
            cmd.extend(['--mount-path', args.mount_path])
        
        if args.esp32_ip:
            cmd.extend(['--esp32-ip', args.esp32_ip])
    
    if not verbose:
        cmd.append('--quiet')
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)


def print_header(title):
    """Print test header."""
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)


def main():
    parser = argparse.ArgumentParser(
        description='Run test suite for MSC-default firmware',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Available Tests:
  upload_simple     - Basic file upload (manual HTTP switch)
  workflow          - Full MSC → HTTP → MSC workflow
  http_while_msc    - Verify HTTP blocked in MSC mode
  msc_while_http    - Verify MSC unavailable after HTTP switch

Examples:
  # Full test suite
  python3 test_all.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST

  # Quick test (critical path)
  python3 test_all.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST \
                      --tests workflow,http_while_msc

  # Skip negative tests
  python3 test_all.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST \
                      --skip-negative
        """
    )
    
    parser.add_argument('--port', required=True,
                       help='Serial port (e.g., /dev/cu.usbmodem53140032081)')
    parser.add_argument('--mount-path', required=True,
                       help='MSC mount path (e.g., /Volumes/TEST)')
    parser.add_argument('--esp32-ip', default='192.168.0.66',
                       help='ESP32 IP')
    parser.add_argument('--tests', default='all',
                       help='Comma-separated list of tests')
    parser.add_argument('--skip-negative', action='store_true',
                       help='Skip negative tests')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode')
    
    args = parser.parse_args()
    
    # Check script directory
    script_dir = Path(__file__).parent
    
    # Header
    print_header("ESP32-S3 MSC-Default Test Suite")
    print(f"ESP32 Mode: MSC (default) with HTTP switch capability")
    print(f"Serial:     {args.port}")
    print(f"Mount:      {args.mount_path}")
    print(f"IP:         {args.esp32_ip}")
    print("=" * 60)
    print()
    print("Prerequisites:")
    print("  1. ESP32 connected via USB")
    print("  2. MSC mounted (default boot state)")
    print("  3. Serial port accessible")
    print()
    
    # Determine which tests to run
    if args.tests == 'all':
        test_list = list(TESTS.keys())
    else:
        test_list = [t.strip() for t in args.tests.split(',')]
    
    if args.skip_negative:
        test_list = [t for t in test_list if not t.startswith('negative')]
    
    # Note about upload_simple
    if 'upload_simple' in test_list:
        print("⚠️  Note: upload_simple requires ESP32 to be in HTTP mode")
        print("   This means you need to manually switch first:")
        print("   1. Send 'h' via serial")
        print("   2. Wait for HTTP to start")
        print()
        response = input("Is ESP32 in HTTP mode? (y/n) or skip this test (s): ")
        if response.lower() != 'y':
            test_list.remove('upload_simple')
            if response.lower() != 's':
                print("   Skipping upload_simple test")
            print()
    
    # Validate tests
    for test in test_list:
        if test not in TESTS:
            print(f"Error: Unknown test '{test}'")
            print(f"Available: {', '.join(TESTS.keys())}")
            sys.exit(1)
    
    # Run tests
    results = {}
    
    for test_name in test_list:
        test_info = TESTS[test_name]
        script = script_dir / test_info['script']
        
        if not script.exists():
            print(f"⚠️  Skipping {test_name}: script not found")
            results[test_name] = None
            continue
        
        print_header(f"{test_info['description']}")
        print(f"Script: {script.name}")
        print()
        
        success, stdout, stderr = run_test(script, args, test_name, not args.quiet)
        
        if not args.quiet:
            if stdout:
                print(stdout)
            if stderr:
                print(stderr, file=sys.stderr)
        
        results[test_name] = success
        
        if success:
            print(f"✅ {test_name}: PASSED")
        else:
            print(f"❌ {test_name}: FAILED")
        
        time.sleep(1)
    
    # Summary
    print_header("TEST SUMMARY")
    
    passed = 0
    failed = 0
    skipped = 0
    
    for test_name, result in results.items():
        if result is None:
            status = "⚠️  SKIPPED"
            skipped += 1
        elif result:
            status = "✅ PASSED"
            passed += 1
        else:
            status = "❌ FAILED"
            failed += 1
        
        print(f"  {test_name:20s} {status}")
    
    print()
    print(f"Total: {passed} passed, {failed} failed, {skipped} skipped")
    print()
    
    if failed == 0:
        print("✅ ALL TESTS PASSED!")
        print("   MSC-default firmware working correctly")
        return 0
    else:
        print("❌ SOME TESTS FAILED")
        print("   Check individual test output above")
        return 1


if __name__ == '__main__':
    sys.exit(main())
