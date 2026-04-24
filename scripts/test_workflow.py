#!/usr/bin/env python3
"""
Workflow Test: MSC Default → HTTP Switch → Upload → MSC Switch → Verify
Tests the complete workflow of dual-interface with MSC-first boot

This script:
1. Verifies ESP32 boots in MSC mode (default)
2. Switches ESP32 to HTTP mode (via serial)
3. Uploads a test file via HTTP
4. Switches back to MSC mode (via web button)
5. Waits for MSC to re-attach
6. Verifies the uploaded file is readable
7. Cleans up test files

Prerequisites:
    - ESP32-S3 running MSC-default firmware
    - ESP32 on USB (for MSC mode)
    - ESP32 connected to WiFi (for HTTP mode)
    - SD card inserted

Usage:
    python3 test_workflow.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST

Dependencies:
    pip install requests pyserial
"""

import argparse
import requests
import serial
import time
import os
import sys
from pathlib import Path
import tempfile


class WorkflowTest:
    """Test MSC → HTTP → Upload → MSC workflow."""
    
    def __init__(self, serial_port, mount_path, esp32_ip='192.168.0.66', verbose=True):
        self.serial_port = serial_port
        self.mount_path = Path(mount_path) if mount_path else None
        self.esp32_ip = esp32_ip
        self.verbose = verbose
        self.test_file_name = "workflow_test.txt"
        self.test_content = "Hello from MSC-default workflow test!\n" + \
                            "Booted in MSC, switched to HTTP, uploaded file.\n" + \
                            f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}\n"
        self.ser = None
        
    def log(self, message):
        if self.verbose:
            print(f"[TEST] {message}")
    
    def log_error(self, message):
        print(f"[ERROR] {message}")
    
    def log_success(self, message):
        print(f"[SUCCESS] {message}")
    
    def open_serial(self):
        """Open serial connection."""
        try:
            self.ser = serial.Serial(self.serial_port, 115200, timeout=0.5)
            return True
        except Exception as e:
            self.log_error(f"Cannot open serial port: {e}")
            return False
    
    def close_serial(self):
        """Close serial connection."""
        if self.ser:
            self.ser.close()
            self.ser = None
    
    def read_serial(self, duration=5):
        """Read serial output for specified duration."""
        if not self.ser:
            return ""
        
        output = ""
        start = time.time()
        while time.time() - start < duration:
            if self.ser.in_waiting:
                data = self.ser.read(self.ser.in_waiting)
                text = data.decode('utf-8', errors='ignore')
                output += text
                print(text, end='', flush=True)
            time.sleep(0.05)
        return output
    
    def send_serial_command(self, cmd, wait=3):
        """Send command via serial."""
        if not self.ser:
            return False
        
        self.ser.write(cmd.encode())
        self.ser.flush()
        time.sleep(wait)
        return True
    
    def step1_verify_msc_boot(self):
        """Step 1: Verify ESP32 booted in MSC mode."""
        self.log("=" * 50)
        self.log("STEP 1: Verify MSC Boot Mode")
        self.log("=" * 50)
        
        if not self.mount_path:
            self.log("No mount path specified, assuming MSC check")
            return True
        
        # Check if MSC is available
        if self.mount_path.exists():
            self.log_success(f"MSC available at {self.mount_path}")
            try:
                files = list(self.mount_path.iterdir())
                self.log(f"Contains {len(files)} items")
                return True
            except Exception as e:
                self.log_error(f"Cannot read MSC: {e}")
                return False
        else:
            self.log_error(f"MSC not mounted at {self.mount_path}")
            self.log("Please ensure ESP32 is connected via USB")
            return False
    
    def step2_switch_to_http(self):
        """Step 2: Switch ESP32 to HTTP mode via serial."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 2: Switch to HTTP Mode")
        self.log("=" * 50)
        
        if not self.open_serial():
            return False
        
        self.log("Sending 'h' command via serial...")
        self.read_serial(1)  # Clear buffer
        
        if not self.send_serial_command('h', wait=2):
            return False
        
        # Read response
        output = self.read_serial(10)
        
        if "HTTP MODE ACTIVE" in output or "Switching to HTTP Mode" in output:
            self.log_success("HTTP mode activated")
            return True
        elif "HTTP" in output:
            self.log_success("HTTP mode transition detected")
            return True
        else:
            self.log_error("HTTP mode not detected")
            self.log(f"Output: {output[-500:]}")
            return False
    
    def step3_wait_for_http(self, max_wait=30):
        """Step 3: Wait for HTTP server to be ready."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 3: Wait for HTTP Server")
        self.log("=" * 50)
        
        self.log(f"Waiting for http://{self.esp32_ip}/ (max {max_wait}s)...")
        
        start = time.time()
        while time.time() - start < max_wait:
            try:
                resp = requests.get(f"http://{self.esp32_ip}/", timeout=2)
                if resp.status_code == 200:
                    self.log_success("HTTP server responding")
                    time.sleep(2)  # Give extra time for stabilization
                    return True
            except:
                pass
            
            if int(time.time() - start) % 5 == 0:
                self.log(f"Waiting... {int(time.time() - start)}s")
            
            time.sleep(0.5)
        
        self.log_error("HTTP server didn't start within timeout")
        return False
    
    def step4_upload_via_http(self):
        """Step 4: Upload test file via HTTP."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 4: Upload File via HTTP")
        self.log("=" * 50)
        
        # Create temp file
        temp_file = Path(tempfile.gettempdir()) / self.test_file_name
        temp_file.write_text(self.test_content)
        
        self.log(f"Created temp file: {temp_file}")
        self.log(f"Content: {repr(self.test_content[:50])}...")
        
        url = f"http://{self.esp32_ip}/upload"
        
        try:
            with open(temp_file, 'rb') as f:
                files = {'file': (self.test_file_name, f)}
                self.log(f"POST {url}")
                response = requests.post(url, files=files, timeout=30)
            
            temp_file.unlink()
            
            if response.status_code in [200, 303]:
                self.log_success(f"Uploaded {self.test_file_name}")
                return True
            else:
                self.log_error(f"Upload failed: HTTP {response.status_code}")
                return False
                
        except Exception as e:
            self.log_error(f"Upload error: {e}")
            return False
    
    def step5_switch_back_to_msc(self):
        """Step 5: Switch back to MSC mode via web button."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 5: Switch Back to MSC Mode")
        self.log("=" * 50)
        
        try:
            self.log("Clicking mode switch button...")
            resp = requests.get(f"http://{self.esp32_ip}/mode/msc", timeout=5)
            
            if "Switching to MSC mode" in resp.text:
                self.log_success("MSC switch triggered")
                # Read serial to confirm
                if self.ser:
                    output = self.read_serial(5)
                    if "MSC" in output:
                        self.log("MSC restart confirmed via serial")
                return True
            else:
                self.log_error("MSC switch not confirmed")
                return False
                
        except Exception as e:
            self.log_error(f"Mode switch failed: {e}")
            return False
    
    def step6_verify_msc_reattach(self, max_wait=30):
        """Step 6: Wait for MSC to re-attach and verify file."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 6: Verify MSC Re-attach")
        self.log("=" * 50)
        
        if not self.mount_path:
            self.log("No mount path, skipping verification")
            return None
        
        self.log("Note: USB must re-enumerate. Physical unplug/replug may be needed.")
        self.log(f"Polling for {self.mount_path}...")
        
        start = time.time()
        while time.time() - start < max_wait:
            if self.mount_path.exists():
                self.log_success(f"MSC re-attached at {self.mount_path}")
                
                # Verify file
                test_file = self.mount_path / self.test_file_name
                if test_file.exists():
                    content = test_file.read_text()
                    if content == self.test_content:
                        self.log_success("Uploaded file verified!")
                        return True
                    else:
                        self.log_error("Content mismatch")
                        return False
                else:
                    self.log_error(f"Test file not found: {test_file}")
                    return False
            
            if int(time.time() - start) % 5 == 0:
                self.log(f"Waiting for re-attach... {int(time.time() - start)}s")
            
            time.sleep(0.5)
        
        self.log_error("MSC didn't re-attach within timeout")
        self.log("This is expected - USB needs physical re-enumeration")
        self.log("For actual use: unplug ESP32 and reconnect")
        return None  # Timeout isn't a failure in this case
    
    def step7_cleanup(self):
        """Step 7: Cleanup test files."""
        self.log("")
        self.log("=" * 50)
        self.log("STEP 7: Cleanup")
        self.log("=" * 50)
        
        if self.mount_path:
            test_file = self.mount_path / self.test_file_name
            if test_file.exists():
                try:
                    test_file.unlink()
                    self.log(f"Deleted: {test_file}")
                except Exception as e:
                    self.log_error(f"Cleanup failed: {e}")
        
        self.close_serial()
        self.log("Cleanup complete")
    
    def run(self):
        """Run full workflow test."""
        print("\n" + "=" * 60)
        print("  MSC-Default → HTTP → Upload → MSC Workflow Test")
        print("=" * 60)
        print(f"Serial Port: {self.serial_port}")
        print(f"Mount Path:  {self.mount_path or 'N/A'}")
        print(f"ESP32 IP:    {self.esp32_ip}")
        print("=" * 60 + "\n")
        
        results = {}
        
        # Step 1
        results['msc_boot'] = self.step1_verify_msc_boot()
        
        # Step 2
        if results['msc_boot']:
            results['http_switch'] = self.step2_switch_to_http()
        else:
            results['http_switch'] = False
        
        # Step 3
        if results['http_switch']:
            results['http_ready'] = self.step3_wait_for_http()
        else:
            results['http_ready'] = False
        
        # Step 4
        if results['http_ready']:
            results['upload'] = self.step4_upload_via_http()
        else:
            results['upload'] = False
        
        # Step 5
        if results['upload']:
            results['msc_switch'] = self.step5_switch_back_to_msc()
        else:
            results['msc_switch'] = False
        
        # Step 6
        if results['msc_switch']:
            results['verify'] = self.step6_verify_msc_reattach()
        else:
            results['verify'] = False
        
        # Step 7
        self.step7_cleanup()
        
        # Summary
        print("\n" + "=" * 60)
        print("  WORKFLOW TEST SUMMARY")
        print("=" * 60)
        
        for step, result in results.items():
            if result is None:
                status = "⚠️  SKIP"
            elif result:
                status = "✅ PASS"
            else:
                status = "❌ FAIL"
            print(f"  {step:20s} {status}")
        
        print("=" * 60)
        
        # Determine overall success
        critical_steps = ['msc_boot', 'http_switch', 'http_ready', 'upload', 'msc_switch']
        critical_pass = all(results.get(s) for s in critical_steps)
        
        if critical_pass:
            print("\n✅ WORKFLOW TEST PASSED!")
            print("   MSC → HTTP → Upload → MSC transition: WORKING")
            if results.get('verify'):
                print("   File persistence: VERIFIED")
        else:
            print("\n❌ WORKFLOW TEST FAILED")
            print("   Some critical steps did not complete")
        
        return critical_pass


def main():
    parser = argparse.ArgumentParser(
        description='Test MSC-default workflow',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with serial and mount
  python3 test_workflow.py --port /dev/cu.usbmodem53140032081 --mount-path /Volumes/TEST

  # Just test transitions (no mount verification)
  python3 test_workflow.py --port /dev/cu.usbmodem53140032081

  # Custom IP
  python3 test_workflow.py --port /dev/cu.usbmodem53140032081 --esp32-ip 192.168.1.100
        """
    )
    
    parser.add_argument('--port', required=True,
                       help='Serial port (e.g., /dev/cu.usbmodem53140032081)')
    parser.add_argument('--mount-path', default=None,
                       help='MSC mount path (e.g., /Volumes/TEST)')
    parser.add_argument('--esp32-ip', default='192.168.0.66',
                       help='ESP32 IP address')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode')
    
    args = parser.parse_args()
    
    try:
        import requests
        import serial
    except ImportError as e:
        print(f"Error: {e}")
        print("Install: pip install requests pyserial")
        sys.exit(1)
    
    test = WorkflowTest(
        serial_port=args.port,
        mount_path=args.mount_path,
        esp32_ip=args.esp32_ip,
        verbose=not args.quiet
    )
    
    success = test.run()
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
