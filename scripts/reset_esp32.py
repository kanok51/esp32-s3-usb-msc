#!/usr/bin/env python3
"""
ESP32-S3 Reset Script - HTTP Always On Architecture
Simple standalone script to reset ESP32 via serial DTR/RTS
Parses boot log to display WiFi IP address and MSC status

New Architecture:
- HTTP server is ALWAYS running
- MSC mode is a toggle (on/off) that controls SD access
- When MSC is ON: SD as USB drive, HTTP shows status
- When MSC is OFF: HTTP can access SD for uploads

Usage:
    python3 scripts/reset_esp32.py
    python3 scripts/reset_esp32.py --port /dev/cu.usbmodem101
"""

import argparse
import serial
import time
import sys
import re


def reset_esp32(port, verbose=True):
    """Reset ESP32 via DTR/RTS toggle."""
    
    if verbose:
        print(f"Opening serial port: {port}")
    
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print(f"\nAvailable ports:")
        import glob
        ports = glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/tty.usbmodem*')
        for p in ports:
            print(f"  {p}")
        return False, None
    
    if verbose:
        print("Sending reset signal...")
        print("  DTR=True, RTS=False")
    
    # Toggle DTR/RTS to reset
    ser.dtr = True
    ser.rts = False
    time.sleep(0.1)
    
    if verbose:
        print("  DTR=False, RTS=True")
    
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    
    if verbose:
        print("  DTR=False, RTS=False")
    
    ser.dtr = False
    ser.rts = False
    time.sleep(0.5)
    
    ser.close()
    
    if verbose:
        print("✅ Reset signal sent")
    
    return True, None


def monitor_boot_and_parse(port, duration=10, verbose=True):
    """Monitor serial output during boot and extract IP and MSC status."""
    
    if verbose:
        print(f"\nMonitoring boot for {duration} seconds...")
        print("-" * 50)
    
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
    except Exception as e:
        if verbose:
            print(f"Could not open port for monitoring: {e}")
        return None
    
    output = ""
    ip_address = None
    msc_enabled = None
    http_running = False
    
    start_time = time.time()
    try:
        while time.time() - start_time < duration:
            data = ser.read(1024)
            if data:
                text = data.decode('utf-8', errors='ignore')
                output += text
                
                # Print in real-time
                if verbose:
                    print(text, end='', flush=True)
                
                # Look for IP address patterns
                ip_match = re.search(r'Connected, IP:\s*([\d.]+)', text)
                if ip_match:
                    ip_address = ip_match.group(1)
                
                # Look for MSC mode
                if "MSC mode enabled" in text or "MSC mode active" in text:
                    msc_enabled = True
                elif "MSC disabled" in text:
                    msc_enabled = False
                
                # Look for HTTP server
                if "HTTP server started" in text:
                    http_running = True
                
            time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    
    ser.close()
    
    if verbose:
        print("\n" + "-" * 50)
    
    return {
        'ip': ip_address,
        'msc_enabled': msc_enabled,
        'http_running': http_running,
        'full_output': output
    }


def continuous_monitor(port):
    """Continuous serial monitor (like screen/picocom)."""
    
    print("\nStarting continuous monitor (press Ctrl+C to exit)...")
    print("-" * 50)
    
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
    except Exception as e:
        print(f"Could not open port: {e}")
        return
    
    try:
        while True:
            data = ser.read(1024)
            if data:
                text = data.decode('utf-8', errors='ignore')
                print(text, end='', flush=True)
            time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    
    ser.close()
    print("\n" + "-" * 50)
    print("Monitor stopped")


def main():
    parser = argparse.ArgumentParser(
        description='Reset ESP32-S3 via serial and display IP/MSC status',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
New Architecture - HTTP Always On:
  - HTTP server ALWAYS runs (for control/status)
  - MSC mode is a toggle that controls SD access
  - MSC ON: SD as USB drive, HTTP shows status
  - MSC OFF: HTTP can access SD for uploads

Examples:
  # Reset and show status (default)
  python3 scripts/reset_esp32.py

  # Reset with custom port
  python3 scripts/reset_esp32.py --port /dev/cu.usbmodem101

  # Reset and monitor boot for 20 seconds
  python3 scripts/reset_esp32.py --wait 20

  # Reset and keep monitoring (Ctrl+C to exit)
  python3 scripts/reset_esp32.py --monitor

  # Quiet reset (just show IP and MSC status)
  python3 scripts/reset_esp32.py --quiet

Output:
  - WiFi IP Address (always shown)
  - MSC Mode Status (enabled/disabled)
  - HTTP Server Status (always running)
        """
    )
    
    parser.add_argument('--port', default='/dev/cu.usbmodem53140032081',
                       help='Serial port (default: /dev/cu.usbmodem53140032081)')
    parser.add_argument('--wait', type=int, default=12,
                       help='Seconds to wait for boot messages (default: 12)')
    parser.add_argument('--monitor', '-m', action='store_true',
                       help='Keep monitoring after reset (Ctrl+C to exit)')
    parser.add_argument('--no-wait', action='store_true',
                       help='Send reset only, don\'t wait for boot')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode - minimal output')
    
    args = parser.parse_args()
    
    # Check if pyserial is available
    try:
        import serial
    except ImportError:
        print("Error: pyserial module not found")
        print("Install: pip install pyserial")
        sys.exit(1)
    
    # Perform reset
    success, _ = reset_esp32(args.port, verbose=not args.quiet)
    
    if not success:
        sys.exit(1)
    
    # Monitor boot if requested
    if args.monitor:
        continuous_monitor(args.port)
    elif not args.no_wait and args.wait > 0:
        result = monitor_boot_and_parse(args.port, args.wait, verbose=not args.quiet)
        
        # Display summary
        if args.quiet:
            # In quiet mode, just print the essentials
            if result and result['ip']:
                print(f"IP: {result['ip']}")
                print(f"HTTP: http://{result['ip']}/")
                if result['msc_enabled'] is not None:
                    print(f"MSC: {'enabled' if result['msc_enabled'] else 'disabled'}")
                else:
                    print("MSC: unknown")
            else:
                print("IP: Not detected")
        else:
            # Normal mode - show summary
            print("\n" + "=" * 50)
            print("BOOT SUMMARY - HTTP Always On Architecture")
            print("=" * 50)
            
            if result:
                ip = result.get('ip', None)
                msc = result.get('msc_enabled', None)
                http = result.get('http_running', False)
                
                if ip:
                    print(f"WiFi IP Address: {ip}")
                    print(f"HTTP Server: http://{ip}/ (always running)")
                    print()
                    
                    if msc is True:
                        print("MSC Mode: ENABLED (SD as USB drive)")
                        print("  - SD card accessible via USB")
                        print("  - HTTP uploads: BLOCKED")
                        print()
                        print(f"To disable MSC (enable uploads): http://{ip}/msc/off")
                    elif msc is False:
                        print("MSC Mode: DISABLED (HTTP access)")
                        print("  - SD card accessible via HTTP uploads")
                        print("  - USB MSC: NOT available")
                        print()
                        print(f"To upload files: http://{ip}/")
                        print(f"To enable MSC (SD as USB): http://{ip}/msc/on")
                    else:
                        print("MSC Mode: Unknown (still booting)")
                        print()
                        print(f"Check status: http://{ip}/api/status")
                    
                    print()
                    print("Serial Commands:")
                    print("  'm' - Enable MSC mode")
                    print("  'h' - Disable MSC mode")
                    print("  's' - Show status")
                else:
                    print("WiFi IP: Not detected (may still be connecting)")
            
            print("=" * 50)
    
    if not args.quiet:
        print("\n✅ ESP32 reset complete")


if __name__ == '__main__':
    main()
