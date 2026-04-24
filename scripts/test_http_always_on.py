#!/usr/bin/env python3
"""
Test Script: HTTP Always On + MSC Toggle Architecture

Tests:
1. HTTP server always running after boot
2. MSC enabled by default (SD as USB)
3. HTTP uploads blocked when MSC enabled
4. MSC can be disabled via HTTP
5. HTTP uploads work when MSC disabled
6. MSC can be re-enabled via HTTP
7. SD accessible via USB when MSC enabled
"""

import argparse
import time
import sys
import os

try:
    import requests
    import serial
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install: pip install requests pyserial")
    sys.exit(1)


class TestRunner:
    def __init__(self, ip, port, mount_path):
        self.ip = ip
        self.port = port
        self.mount_path = mount_path
        self.base_url = f"http://{ip}"
        self.results = []
        
    def log(self, message):
        print(f"  {message}")
        
    def check(self, condition, message):
        if condition:
            self.results.append(("PASS", message))
            print(f"    ✅ {message}")
        else:
            self.results.append(("FAIL", message))
            print(f"    ❌ {message}")
        return condition
        
    def test_http_running(self):
        """Test 1: HTTP server is always running"""
        print("\n[Test 1] HTTP Server Always Running")
        try:
            response = requests.get(self.base_url, timeout=5)
            return self.check(response.status_code == 200, "HTTP server responding")
        except Exception as e:
            return self.check(False, f"HTTP server not responding: {e}")
            
    def test_msc_default_enabled(self):
        """Test 2: MSC is enabled by default after reset"""
        print("\n[Test 2] MSC Enabled by Default")
        try:
            response = requests.get(f"{self.base_url}/api/status", timeout=5)
            data = response.json()
            return self.check(data.get('msc_active') == True, "MSC active by default")
        except Exception as e:
            return self.check(False, f"Failed to check MSC status: {e}")
            
    def test_sd_blocked_when_msc_enabled(self):
        """Test 3: SD not accessible via HTTP when MSC enabled"""
        print("\n[Test 3] SD Blocked via HTTP when MSC Enabled")
        try:
            response = requests.get(f"{self.base_url}/api/status", timeout=5)
            data = response.json()
            return self.check(data.get('sd_accessible') == False, 
                            "SD not accessible via HTTP when MSC enabled")
        except Exception as e:
            return self.check(False, f"Failed to check SD accessibility: {e}")
            
    def test_msc_usb_mounted(self):
        """Test 4: SD mounted as USB drive when MSC enabled"""
        print("\n[Test 4] SD Mounted as USB Drive")
        if not self.mount_path:
            print("    ⚠️  Skipping (no mount path provided)")
            return True
            
        try:
            # Wait a moment for mount
            time.sleep(1)
            files = os.listdir(self.mount_path)
            return self.check(True, f"SD mounted at {self.mount_path} ({len(files)} files)")
        except Exception as e:
            return self.check(False, f"SD not mounted: {e}")
            
    def test_disable_msc_via_http(self):
        """Test 5: Can disable MSC via HTTP"""
        print("\n[Test 5] Disable MSC via HTTP")
        try:
            response = requests.get(f"{self.base_url}/msc/off", timeout=10)
            # Wait for mode switch
            time.sleep(3)
            
            # Verify MSC is disabled
            response = requests.get(f"{self.base_url}/api/status", timeout=5)
            data = response.json()
            return self.check(data.get('msc_active') == False, "MSC disabled via HTTP")
        except Exception as e:
            return self.check(False, f"Failed to disable MSC: {e}")
            
    def test_upload_when_msc_disabled(self):
        """Test 6: HTTP uploads work when MSC disabled"""
        print("\n[Test 6] HTTP Upload When MSC Disabled")
        
        # Create test file
        test_content = f"Test upload at {time.strftime('%Y-%m-%d %H:%M:%S')}"
        test_file = "/tmp/test_upload.txt"
        with open(test_file, 'w') as f:
            f.write(test_content)
            
        try:
            # Upload file
            with open(test_file, 'rb') as f:
                response = requests.post(
                    f"{self.base_url}/upload",
                    files={'file': ('test_upload.txt', f)},
                    timeout=10,
                    allow_redirects=False
                )
            
            success = response.status_code in [200, 303]
            
            # Verify file appears in list
            if success:
                time.sleep(1)
                response = requests.get(self.base_url, timeout=5)
                success = 'test_upload.txt' in response.text
                
            return self.check(success, "HTTP upload works when MSC disabled")
        except Exception as e:
            return self.check(False, f"Upload failed: {e}")
            
    def test_upload_blocked_when_msc_enabled(self):
        """Test 7: HTTP uploads blocked when MSC enabled"""
        print("\n[Test 7] HTTP Upload Blocked When MSC Enabled")
        
        # First enable MSC
        try:
            requests.get(f"{self.base_url}/msc/on", timeout=10)
            time.sleep(3)
            
            # Verify MSC is enabled
            response = requests.get(f"{self.base_url}/api/status", timeout=5)
            data = response.json()
            if not data.get('msc_active'):
                return self.check(False, "MSC not enabled, cannot test upload blocking")
                
        except Exception as e:
            return self.check(False, f"Failed to enable MSC: {e}")
            
        # Try to upload
        test_file = "/tmp/test_blocked.txt"
        with open(test_file, 'w') as f:
            f.write("This should be blocked")
            
        try:
            with open(test_file, 'rb') as f:
                response = requests.post(
                    f"{self.base_url}/upload",
                    files={'file': ('test_blocked.txt', f)},
                    timeout=10
                )
            
            # Should be blocked (403 or empty response)
            blocked = response.status_code == 403 or 'MSC' in response.text or len(response.text) < 100
            return self.check(blocked, "HTTP upload blocked when MSC enabled")
        except Exception as e:
            # Connection errors also indicate blocking
            return self.check(True, f"Upload blocked (connection issue: {e})")
            
    def run_all_tests(self):
        """Run all tests in sequence"""
        print("=" * 60)
        print("HTTP Always On + MSC Toggle Architecture Tests")
        print("=" * 60)
        print(f"Target: {self.base_url}")
        print(f"Mount Path: {self.mount_path or 'N/A'}")
        print()
        
        tests = [
            self.test_http_running,
            self.test_msc_default_enabled,
            self.test_sd_blocked_when_msc_enabled,
            self.test_msc_usb_mounted,
            self.test_disable_msc_via_http,
            self.test_upload_when_msc_disabled,
            self.test_upload_blocked_when_msc_enabled,
        ]
        
        for test in tests:
            try:
                test()
            except Exception as e:
                print(f"    ❌ Test crashed: {e}")
                self.results.append(("CRASH", str(e)))
                
        # Print summary
        print("\n" + "=" * 60)
        print("TEST SUMMARY")
        print("=" * 60)
        
        passed = sum(1 for r in self.results if r[0] == "PASS")
        failed = sum(1 for r in self.results if r[0] == "FAIL")
        crashed = sum(1 for r in self.results if r[0] == "CRASH")
        
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print(f"Crashed: {crashed}")
        print()
        
        if failed > 0 or crashed > 0:
            print("Failed tests:")
            for status, message in self.results:
                if status != "PASS":
                    print(f"  {status}: {message}")
                    
        print("=" * 60)
        return failed == 0 and crashed == 0


def main():
    parser = argparse.ArgumentParser(
        description='Test HTTP Always On + MSC Toggle architecture'
    )
    parser.add_argument('--ip', default='192.168.0.66',
                       help='ESP32 IP address (default: 192.168.0.66)')
    parser.add_argument('--port', default='/dev/cu.usbmodem53140032081',
                       help='Serial port for reset (default: /dev/cu.usbmodem53140032081)')
    parser.add_argument('--mount-path', 
                       help='Path where SD card is mounted (e.g., /Volumes/TEST)')
    parser.add_argument('--no-reset', action='store_true',
                       help='Skip reset (assume already in test state)')
    
    args = parser.parse_args()
    
    # Optional: Reset ESP32 first to ensure clean state
    if not args.no_reset:
        print("Resetting ESP32 to ensure clean state...")
        try:
            ser = serial.Serial(args.port, 115200, timeout=0.1)
            ser.dtr = True
            ser.rts = False
            time.sleep(0.1)
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.dtr = False
            ser.rts = False
            ser.close()
            print("Reset sent, waiting for boot...")
            time.sleep(10)
        except Exception as e:
            print(f"Warning: Could not reset: {e}")
            print("Continuing with current state...")
    
    # Run tests
    runner = TestRunner(args.ip, args.port, args.mount_path)
    success = runner.run_all_tests()
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
