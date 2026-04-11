#!/usr/bin/env python3
"""
Test script for PR #6: FTP service module

Tests:
1. Connect to FTP server
2. Upload a file
3. Download the file
4. Verify content

Usage:
    python3 scripts/test_ftp.py --host 192.168.0.66 --user esp32 --password esp32pass

Dependencies:
    pip install pyserial
"""

import argparse
import ftplib
import io
import sys
import time


def test_ftp_connection(host: str, user: str, password: str, timeout: int = 30) -> bool:
    """Test: Connect to FTP server."""
    print("\n=== Test: FTP Connection ===")
    print(f"Connecting to {host}...")
    
    start = time.time()
    while time.time() - start < timeout:
        try:
            ftp = ftplib.FTP(host, timeout=5)
            ftp.login(user, password)
            print(f"Connected successfully")
            print(f"Welcome: {ftp.getwelcome()}")
            ftp.quit()
            print("Result: PASS\n")
            return True
        except Exception as e:
            print(f"  Waiting... ({int(time.time() - start)}s): {e}")
            time.sleep(2)
    
    print(f"Result: FAIL - Could not connect within {timeout}s\n")
    return False


def test_ftp_upload_download(host: str, user: str, password: str) -> bool:
    """Test: Upload and download a file."""
    print("\n=== Test: FTP Upload/Download ===")
    
    test_content = "Hello from FTP test script!\nLine 2\n"
    test_filename = "_ftp_test.txt"
    
    try:
        ftp = ftplib.FTP(host, timeout=10)
        ftp.login(user, password)
        print("Logged in")
        
        # List current directory
        print("Current files:")
        ftp.retrlines('LIST')
        
        # Upload file
        print(f"\nUploading {test_filename}...")
        bio = io.BytesIO(test_content.encode('utf-8'))
        ftp.storbinary(f'STOR {test_filename}', bio)
        print("Upload complete")
        
        # Download file
        print(f"\nDownloading {test_filename}...")
        download_bio = io.BytesIO()
        ftp.retrbinary(f'RETR {test_filename}', download_bio.write)
        downloaded_content = download_bio.getvalue().decode('utf-8')
        print("Download complete")
        
        # Verify content
        if downloaded_content == test_content:
            print("Content verified: MATCH")
        else:
            print(f"Content MISMATCH:")
            print(f"  Expected: {repr(test_content)}")
            print(f"  Got: {repr(downloaded_content)}")
            ftp.delete(test_filename)
            ftp.quit()
            print("Result: FAIL\n")
            return False
        
        # Cleanup
        ftp.delete(test_filename)
        print("Test file deleted")
        
        ftp.quit()
        print("Result: PASS\n")
        return True
        
    except Exception as e:
        print(f"FTP error: {e}")
        print("Result: FAIL\n")
        return False


def main():
    parser = argparse.ArgumentParser(description="Test PR #6: FTP Service")
    parser.add_argument("--host", default="192.168.0.66", help="FTP server IP")
    parser.add_argument("--user", default="esp32", help="FTP username")
    parser.add_argument("--password", default="esp32pass", help="FTP password")
    parser.add_argument("--timeout", type=int, default=30, help="Connection timeout")
    args = parser.parse_args()
    
    print(f"Testing FTP on {args.host}")
    print(f"Credentials: {args.user} / {'*' * len(args.password)}")
    
    # Test 1: Connection
    conn_ok = test_ftp_connection(args.host, args.user, args.password, args.timeout)
    
    # Test 2: Upload/Download
    transfer_ok = test_ftp_upload_download(args.host, args.user, args.password) if conn_ok else False
    
    # Summary
    print("=" * 50)
    print("SUMMARY")
    print("=" * 50)
    print(f"FTP Connection:       {'PASS' if conn_ok else 'FAIL'}")
    print(f"FTP Upload/Download:  {'PASS' if transfer_ok else 'FAIL'}")
    
    all_pass = conn_ok and transfer_ok
    print(f"\nOverall: {'ALL TESTS PASSED' if all_pass else 'SOME TESTS FAILED'}")
    
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
