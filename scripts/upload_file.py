#!/usr/bin/env python3
"""
Simple file upload script for ESP32-S3 SD card over WiFi HTTP
Usage:
    python3 upload_file.py <file_to_upload>
    python3 upload_file.py <file_to_upload> --ip 192.168.0.66
    python3 upload_file.py <file_to_upload> --remote-name custom.txt
"""

import argparse
import requests
import sys
from pathlib import Path


def upload_file(file_path, esp32_ip, remote_name=None, verbose=True):
    """Upload a file to ESP32 HTTP server."""
    
    file_path = Path(file_path)
    if not file_path.exists():
        print(f"Error: File not found: {file_path}")
        return False
    
    if remote_name is None:
        remote_name = file_path.name
    
    url = f"http://{esp32_ip}/upload"
    
    if verbose:
        print(f"Uploading: {file_path} ({file_path.stat().st_size} bytes)")
        print(f"To: http://{esp32_ip}/")
        print(f"As: {remote_name}")
    
    try:
        with open(file_path, 'rb') as f:
            files = {'file': (remote_name, f)}
            response = requests.post(url, files=files, timeout=30)
        
        if response.status_code == 200 or response.status_code == 303:
            if verbose:
                print(f"✅ Upload successful!")
            return True
        else:
            if verbose:
                print(f"❌ Upload failed: HTTP {response.status_code}")
            return False
            
    except requests.exceptions.ConnectionError:
        print(f"❌ Cannot connect to {esp32_ip}")
        print("   Is the ESP32 on the same network?")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False


def list_files(esp32_ip):
    """List files on SD card via HTTP GET."""
    
    try:
        response = requests.get(f"http://{esp32_ip}/", timeout=10)
        if response.status_code == 200:
            # Parse HTML to extract file list
            import re
            files = re.findall(r'<li>([^<]+) \(\d+ bytes\)', response.text)
            return files
        return []
    except:
        return []


def main():
    parser = argparse.ArgumentParser(
        description='Upload file to ESP32-S3 SD card via HTTP',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Upload a file
  python3 upload_file.py mydata.txt

  # Upload to custom IP
  python3 upload_file.py mydata.txt --ip 192.168.1.100

  # Upload with different remote name
  python3 upload_file.py local.txt --remote-name device_data.txt

  # Upload without messages (quiet mode)
  python3 upload_file.py mydata.txt --quiet
        """
    )
    
    parser.add_argument('file', help='Path to file to upload')
    parser.add_argument('--ip', default='192.168.0.66',
                       help='ESP32 IP address (default: 192.168.0.66)')
    parser.add_argument('--remote-name', default=None,
                       help='Remote filename (default: same as local)')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode - only show errors')
    
    args = parser.parse_args()
    
    # Check if requests module is available
    try:
        import requests
    except ImportError:
        print("Error: 'requests' module not found.")
        print("Install it: pip install requests")
        sys.exit(1)
    
    # Upload file
    success = upload_file(
        args.file,
        args.ip,
        args.remote_name,
        verbose=not args.quiet
    )
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
