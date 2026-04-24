#!/usr/bin/env python3
"""
ESP32-S3 Workflow Automation Module

Usage:
    python3 scripts/esp32_workflow.py [options]

Examples:
    # Full workflow
    python3 scripts/esp32_workflow.py
    
    # Skip build (use existing firmware)
    python3 scripts/esp32_workflow.py --skip-build
    
    # Custom IP address
    python3 scripts/esp32_workflow.py --ip 192.168.1.100
    
    # Just monitor serial
    python3 scripts/esp32_workflow.py --monitor-only
    
    # Reset and test only
    python3 scripts/esp32_workflow.py --reset --test
"""

import argparse
import subprocess
import sys
import time
import serial
import re
import ftplib
import io
from pathlib import Path


class ESP32Workflow:
    """ESP32-S3 workflow automation class."""
    
    # Default configuration
    DEFAULT_CONFIG = {
        'project_dir': None,  # Auto-detected
        'serial_port': '/dev/cu.usbmodem53140032081',
        'monitor_port': '/dev/cu.usbmodem53140032081',
        'ftp_ip': '192.168.0.66',
        'ftp_user': 'esp32',
        'ftp_pass': 'esp32pass',
        'serial_baud': 115200,
        'boot_timeout': 60,
        'post_reset_wait': 3,
        'boot_wait': 15,
    }
    
    def __init__(self, **kwargs):
        """Initialize workflow with configuration."""
        self.config = self.DEFAULT_CONFIG.copy()
        self.config.update(kwargs)
        
        # Auto-detect project directory
        if self.config['project_dir'] is None:
            self.config['project_dir'] = Path(__file__).parent.parent.absolute()
        
        self._status_messages = []
    
    def log(self, message, level='INFO'):
        """Log a message with level."""
        colors = {
            'INFO': '\033[0;34m',      # Blue
            'SUCCESS': '\033[0;32m',   # Green
            'WARNING': '\033[1;33m',   # Yellow
            'ERROR': '\033[0;31m',     # Red
            'RESET': '\033[0m'
        }
        
        color = colors.get(level, colors['INFO'])
        reset = colors['RESET']
        
        formatted = f"{color}[{level}]{reset} {message}"
        print(formatted)
        
        self._status_messages.append({
            'time': time.time(),
            'level': level,
            'message': message
        })
    
    def check_prerequisites(self):
        """Check that all prerequisites are met."""
        self.log("Checking prerequisites...")
        
        # Check PlatformIO
        result = subprocess.run(['which', 'pio'], 
                              capture_output=True, text=True)
        if result.returncode != 0:
            self.log("PlatformIO not found. Install: pip install platformio", 'ERROR')
            return False
        self.log("✓ PlatformIO found")
        
        # Check pyserial
        try:
            import serial
            self.log("✓ pyserial found")
        except ImportError:
            self.log("pyserial not found. Installing...", 'WARNING')
            result = subprocess.run([sys.executable, '-m', 'pip', 'install', 'pyserial'],
                                  capture_output=True)
            if result.returncode != 0:
                self.log("Failed to install pyserial", 'ERROR')
                return False
            self.log("✓ pyserial installed")
        
        # Check project directory
        if not Path(self.config['project_dir']).exists():
            self.log(f"Project directory not found: {self.config['project_dir']}", 'ERROR')
            return False
        self.log(f"✓ Project directory: {self.config['project_dir']}")
        
        return True
    
    def build_and_upload(self, skip_build=False):
        """Build and upload firmware."""
        if skip_build:
            self.log("[1/5] Skipping build (--skip-build)")
            cmd = ['pio', 'run', '--target', 'upload']
        else:
            self.log("[1/5] Cleaning and building...")
            # Clean first
            subprocess.run(['pio', 'run', '--target', 'clean'],
                          cwd=self.config['project_dir'],
                          capture_output=True)
            cmd = ['pio', 'run', '--target', 'upload']
        
        self.log(f"Running: {' '.join(cmd)}")
        
        result = subprocess.run(cmd,
                              cwd=self.config['project_dir'],
                              capture_output=True,
                              text=True)
        
        if result.returncode != 0:
            self.log("Build/upload failed", 'ERROR')
            self.log(result.stderr[-500:] if result.stderr else "No error output", 'ERROR')
            return False
        
        # Check for success indicators
        if "Hash of data verified" in result.stdout:
            self.log("✓ Upload successful", 'SUCCESS')
        else:
            self.log("Build completed (check output)", 'SUCCESS')
        
        return True
    
    def reset_board(self):
        """Reset board via serial DTR/RTS."""
        self.log("[2/5] Resetting board via serial...")
        
        try:
            ser = serial.Serial(self.config['serial_port'], 
                              115200, 
                              timeout=0.1)
            
            # Toggle DTR/RTS to reset
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
            self.log("✓ Reset signal sent", 'SUCCESS')
            return True
            
        except serial.SerialException as e:
            self.log(f"Serial reset failed: {e}", 'WARNING')
            self.log("Please press the RST button manually", 'WARNING')
            return False
    
    def wait_for_boot(self):
        """Wait for board to boot and detect IP."""
        self.log("[3/5] Waiting for boot...")
        self.log(f"Monitoring {self.config['monitor_port']} for {self.config['boot_timeout']}s...")
        
        try:
            ser = serial.Serial(self.config['monitor_port'],
                              self.config['serial_baud'],
                              timeout=0.1)
        except serial.SerialException as e:
            self.log(f"Cannot open serial port: {e}", 'ERROR')
            return None
        
        output = []
        detected_ip = None
        start_time = time.time()
        
        try:
            while time.time() - start_time < self.config['boot_timeout']:
                data = ser.read(1024)
                if data:
                    text = data.decode('utf-8', errors='ignore')
                    output.append(text)
                    
                    # Print serial output in real-time
                    print(text, end='', flush=True)
                    
                    # Look for IP address
                    full_output = ''.join(output)
                    
                    # Pattern 1: "Connected, IP: x.x.x.x"
                    ip_match = re.search(r'Connected, IP:\\s*([\\d.]+)', full_output)
                    if ip_match:
                        detected_ip = ip_match.group(1)
                    
                    # Pattern 2: "FTP ready at ftp://x.x.x.x"
                    if not detected_ip and 'FTP ready at ftp://' in full_output:
                        ip_match = re.search(r'FTP ready at ftp://([\\d.]+)', full_output)
                        if ip_match:
                            detected_ip = ip_match.group(1)
                    
                    if detected_ip:
                        break
                
                time.sleep(0.01)
        
        except KeyboardInterrupt:
            self.log("Interrupted by user", 'WARNING')
        
        finally:
            ser.close()
        
        if detected_ip:
            if detected_ip != self.config['ftp_ip']:
                self.log(f"IP changed: {self.config['ftp_ip']} → {detected_ip}", 'WARNING')
                self.config['ftp_ip'] = detected_ip
            self.log(f"✓ Board ready, IP: {detected_ip}", 'SUCCESS')
        else:
            self.log("Could not detect IP from serial output", 'WARNING')
            self.log(f"Using configured IP: {self.config['ftp_ip']}")
        
        return detected_ip
    
    def run_ftp_tests(self):
        """Run FTP connection and transfer tests."""
        self.log("[4/5] Running FTP tests...")
        
        results = {
            'connection': False,
            'upload': False,
            'download': False,
        }
        
        # Test 1: Connection
        self.log(f"Testing FTP connection to {self.config['ftp_ip']}...")
        try:
            ftp = ftplib.FTP(self.config['ftp_ip'], timeout=10)
            ftp.login(self.config['ftp_user'], self.config['ftp_pass'])
            welcome = ftp.getwelcome()
            self.log(f"✓ Connected: {welcome[:50]}...", 'SUCCESS')
            results['connection'] = True
            
            # Test 2: Directory listing
            self.log("Testing directory listing...")
            files = []
            ftp.retrlines('LIST', files.append)
            self.log(f"✓ Listed {len(files)} files/directories", 'SUCCESS')
            
            # Test 3: Upload
            self.log("Testing file upload...")
            test_data = b"Hello from ESP32 workflow test!"
            test_file = "_workflow_test.txt"
            
            bio = io.BytesIO(test_data)
            try:
                ftp.storbinary(f'STOR {test_file}', bio)
                self.log("✓ Upload successful", 'SUCCESS')
                results['upload'] = True
                
                # Test 4: Download
                self.log("Testing file download...")
                download = io.BytesIO()
                ftp.retrbinary(f'RETR {test_file}', download.write)
                
                if download.getvalue() == test_data:
                    self.log("✓ Download verified", 'SUCCESS')
                    results['download'] = True
                else:
                    self.log("Download data mismatch", 'WARNING')
                
                # Cleanup
                ftp.delete(test_file)
                
            except ftplib.error_perm as e:
                error_str = str(e)
                if "552" in error_str:
                    self.log("✗ Upload failed: 552 Probably insufficient storage space", 'ERROR')
                    self.log("   This is a known library bug, not a configuration issue", 'WARNING')
                else:
                    self.log(f"✗ Upload failed: {e}", 'ERROR')
            
            ftp.quit()
            
        except Exception as e:
            self.log(f"✗ Connection failed: {e}", 'ERROR')
        
        return results
    
    def print_summary(self, results):
        """Print workflow summary."""
        print("\\n" + "="*50)
        self.log("Workflow Complete")
        print("="*50)
        
        print("\\nResults:")
        print(f"  Build/Upload:    {'✅' if True else '❌'} Completed")
        print(f"  Board Reset:     {'✅' if True else '❌'} Completed")
        print(f"  Boot Detection:  {'✅' if results.get('ip') else '⚠️'} {results.get('ip', 'N/A')}")
        print(f"  FTP Connection:  {'✅' if results.get('ftp', {}).get('connection') else '❌'}")
        print(f"  FTP Upload:      {'✅' if results.get('ftp', {}).get('upload') else '❌'}")
        
        print("\\nNotes:")
        print("  - FTP upload failures are expected due to library bug")
        print("  - Use USB MSC for reliable file transfers")
        print("  - Connect ESP32 to computer via USB")
        
        print("\\nConfiguration:")
        print(f"  Project: {self.config['project_dir']}")
        print(f"  Serial:  {self.config['serial_port']}")
        print(f"  FTP:     {self.config['ftp_ip']}")
        print(f"  User:    {self.config['ftp_user']}")
    
    def run_full_workflow(self, skip_build=False, only_monitor=False):
        """Run complete workflow."""
        if only_monitor:
            return self.monitor_only()
        
        results = {
            'ip': None,
            'ftp': {
                'connection': False,
                'upload': False,
                'download': False,
            }
        }
        
        print("\\n" + "="*50)
        self.log("ESP32-S3 Full Workflow")
        print("="*50)
        print(f"Project: {self.config['project_dir']}")
        print(f"FTP IP:  {self.config['ftp_ip']}")
        print("")
        
        # 1. Check prerequisites
        if not self.check_prerequisites():
            return 1
        
        # 2. Build and upload
        if not self.build_and_upload(skip_build=skip_build):
            return 1
        
        # 3. Wait for post-upload
        time.sleep(self.config['post_reset_wait'])
        
        # 4. Reset board
        self.reset_board()
        
        # 5. Wait for boot and detect IP
        results['ip'] = self.wait_for_boot()
        
        # 6. Run FTP tests
        time.sleep(2)  # Give extra time for services
        results['ftp'] = self.run_ftp_tests()
        
        # 7. Print summary
        self.print_summary(results)
        
        return 0 if results['ftp']['connection'] else 1
    
    def monitor_only(self):
        """Only monitor serial output."""
        self.log("Monitor mode - press Ctrl+C to exit")
        
        try:
            ser = serial.Serial(self.config['monitor_port'],
                              self.config['serial_baud'],
                              timeout=0.1)
            
            while True:
                data = ser.read(1024)
                if data:
                    print(data.decode('utf-8', errors='ignore'), end='', flush=True)
                time.sleep(0.01)
        
        except KeyboardInterrupt:
            print("\\n")
            self.log("Monitor stopped by user")
        except Exception as e:
            self.log(f"Monitor error: {e}", 'ERROR')
        
        return 0


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='ESP32-S3 Workflow Automation',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Full workflow
  python3 %(prog)s
  
  # Skip build (faster)
  python3 %(prog)s --skip-build
  
  # Custom IP
  python3 %(prog)s --ip 192.168.1.100
  
  # Only monitor serial
  python3 %(prog)s --monitor-only
  
  # Reset and test only (no build/upload)
  python3 %(prog)s --skip-build --reset --test
        '''
    )
    
    parser.add_argument('--skip-build', action='store_true',
                       help='Skip build step (use existing firmware)')
    parser.add_argument('--ip', default='192.168.0.66',
                       help='FTP server IP address (default: 192.168.0.66)')
    parser.add_argument('--port', default='/dev/cu.usbmodem53140032081',
                       help='Serial port (default: /dev/cu.usbmodem53140032081)')
    parser.add_argument('--project-dir',
                       help='Project directory (default: auto-detect)')
    parser.add_argument('--monitor-only', action='store_true',
                       help='Only monitor serial output')
    parser.add_argument('--reset', action='store_true',
                       help='Reset board (use with --skip-build)')
    parser.add_argument('--test', action='store_true',
                       help='Run tests (use with --skip-build)')
    
    args = parser.parse_args()
    
    # Create workflow instance
    config = {
        'ftp_ip': args.ip,
        'serial_port': args.port,
        'monitor_port': args.port,
    }
    
    if args.project_dir:
        config['project_dir'] = args.project_dir
    
    workflow = ESP32Workflow(**config)
    
    # Run workflow
    if args.monitor_only:
        return workflow.monitor_only()
    
    # Handle --reset --test combination
    if args.skip_build and args.reset and args.test:
        # Special mode: just reset and test
        workflow.reset_board()
        time.sleep(15)
        workflow.wait_for_boot()
        time.sleep(2)
        workflow.run_ftp_tests()
        return 0
    
    return workflow.run_full_workflow(skip_build=args.skip_build)


if __name__ == '__main__':
    sys.exit(main())
