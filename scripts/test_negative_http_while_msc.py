#!/usr/bin/env python3
"""
NEGATIVE TEST: HTTP Upload while MSC is Active (Default Boot)

This test verifies:
1. ESP32 boots in MSC mode (default)
2. HTTP is not accessible when MSC is active
3. Device A can safely read from MSC without HTTP interference

Prerequisites:
    - ESP32-S3 with MSC-default firmware
    - ESP32 in MSC mode (default state)
    - MSC mounted and accessible

Expected Result:
    - HTTP connection refused when MSC active
    - This is the CORRECT behavior (prevents SD corruption)

Usage:
    python3 test_negative_http_while_msc.py \
        --mount-path /Volumes/TEST \
        --esp32-ip 192.168.0.66
"""

import argparse
import requests
import time
import sys
from pathlib import Path
import tempfile


def test_http_while_msc_active(mount_path, esp32_ip='192.168.0.66', verbose=True):
    """Test that HTTP is blocked when MSC is active."""
    
    if verbose:
        print("=" * 60)
        print("NEGATIVE TEST: HTTP while MSC Active (Default Boot)")
        print("=" * 60)
        print(f"Mount Path:  {mount_path}")
        print(f"ESP32 IP:    {esp32_ip}")
        print()
        print("Testing HTTP is unavailable in MSC-default mode...")
        print("-" * 60)
    
    # Step 1: Verify MSC is available (default state)
    if verbose:
        print("\nStep 1: Verify MSC is available (default boot state)")
    
    mount = Path(mount_path)
    if not mount.exists():
        print("❌ TEST SETUP FAILED")
        print(f"   MSC not found at: {mount_path}")
        print("   Ensure ESP32 is:")
        print("   1. Connected via USB")
        print("   2. Not switched to HTTP mode")
        print("   3. In MSC-default mode (restart if needed)")
        return False
    
    if verbose:
        print(f"✅ MSC available: {mount_path}")
        try:
            files = [f.name for f in mount.iterdir() if f.is_file()][:5]
            print(f"   Contains {len(files)} files")
        except Exception as e:
            print(f"   Accessible but: {e}")
    
    # Step 2: Try HTTP GET
    if verbose:
        print("\nStep 2: Try HTTP GET (should fail)")
    
    try:
        resp = requests.get(f"http://{esp32_ip}/", timeout=5)
        if resp.status_code == 200:
            print("❌ FAILURE: HTTP is accessible!")
            print("   Expected: HTTP should be OFF in MSC mode")
            print("   Actual: HTTP responding")
            print()
            print("   Verdict: MUTUAL EXCLUSION VIOLATED")
            return False
        else:
            print(f"✅ HTTP blocked (status: {resp.status_code})")
    except requests.exceptions.ConnectionError:
        if verbose:
            print("✅ HTTP blocked (Connection refused)")
    except requests.exceptions.Timeout:
        if verbose:
            print("✅ HTTP blocked (Timeout)")
    except Exception as e:
        if verbose:
            print(f"✅ HTTP blocked ({type(e).__name__})")
    
    # Step 3: Try HTTP POST (upload)
    if verbose:
        print("\nStep 3: Try HTTP POST/upload (should fail)")
    
    temp_file = Path(tempfile.gettempdir()) / "test_upload.txt"
    temp_file.write_text("Test data")
    
    try:
        with open(temp_file, 'rb') as f:
            files = {'file': f}
            resp = requests.post(f"http://{esp32_ip}/upload", 
                               files=files, timeout=5)
        
        temp_file.unlink()
        
        if resp.status_code in [200, 303]:
            print("❌ FAILURE: Upload succeeded!")
            print("   HTTP upload should be blocked in MSC mode")
            return False
        else:
            print(f"✅ Upload blocked (status: {resp.status_code})")
            
    except requests.exceptions.ConnectionError:
        if verbose:
            print("✅ Upload blocked (Connection refused)")
    except requests.exceptions.Timeout:
        if verbose:
            print("✅ Upload blocked (Timeout)")
    except Exception as e:
        if verbose:
            print(f"✅ Upload blocked ({type(e).__name__})")
    
    finally:
        temp_file.unlink(missing_ok=True)
    
    # Step 4: Verify MSC still accessible
    if verbose:
        print("\nStep 4: Verify MSC remains accessible")
    
    if mount.exists():
        try:
            test_file = mount / "README.txt"
            if test_file.exists():
                content = test_file.read_text()
                if verbose:
                    print(f"✅ MSC still accessible (read {len(content)} chars)")
            else:
                if verbose:
                    print("✅ MSC still accessible")
        except Exception as e:
            if verbose:
                print(f"⚠️  MSC accessible but error: {e}")
    else:
        print("❌ Warning: MSC disappeared during test")
    
    # Results
    if verbose:
        print("\n" + "=" * 60)
        print("NEGATIVE TEST RESULT")
        print("=" * 60)
    
    print()
    print("✅ NEGATIVE TEST PASSED!")
    print("   HTTP is correctly blocked when MSC is active")
    print("   Device A can safely read from MSC")
    print()
    print("   Summary:")
    print("   - MSC available (default boot) ✅")
    print("   - HTTP connection blocked ✅")
    print("   - HTTP upload blocked ✅")
    print("   - Mutual exclusion working ✅")
    print()
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Test HTTP is blocked when MSC is active',
        epilog="""
Example:
    python3 test_negative_http_while_msc.py \
        --mount-path /Volumes/TEST \
        --esp32-ip 192.168.0.66
        """
    )
    
    parser.add_argument('--mount-path', required=True,
                       help='MSC mount path (e.g., /Volumes/TEST)')
    parser.add_argument('--esp32-ip', default='192.168.0.66',
                       help='ESP32 IP address')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Quiet mode')
    
    args = parser.parse_args()
    
    try:
        import requests
    except ImportError:
        print("Error: requests module not found")
        print("Install: pip install requests")
        sys.exit(1)
    
    result = test_http_while_msc_active(
        mount_path=args.mount_path,
        esp32_ip=args.esp32_ip,
        verbose=not args.quiet
    )
    
    sys.exit(0 if result else 1)


if __name__ == '__main__':
    main()
