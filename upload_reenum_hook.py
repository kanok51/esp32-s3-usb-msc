from pathlib import Path
import os
import time

Import("env")

try:
    import serial
except ImportError:
    print("pyserial is required for upload_reenum_hook.py")
    raise

INITIAL_SUFFIX = "usbmodem53140032081"
RESET_SUFFIX   = "usbmodem2201"


def detect_prefix():
    env_prefix = os.environ.get("ESP_PORT_PREFIX")
    if env_prefix in ("/dev/cu.", "/dev/tty."):
        return env_prefix

    for prefix in ("/dev/cu.", "/dev/tty."):
        if Path(prefix + INITIAL_SUFFIX).exists():
            return prefix

    for prefix in ("/dev/cu.", "/dev/tty."):
        if Path(prefix + RESET_SUFFIX).exists():
            return prefix

    return "/dev/tty."


def wait_for_port(port, timeout_s=10.0):
    end = time.time() + timeout_s
    while time.time() < end:
        if Path(port).exists():
            return True
        time.sleep(0.2)
    return False


def force_bootloader_reset(port):
    ser = serial.Serial(port, 115200)
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.10)

        ser.dtr = True
        ser.rts = False
        time.sleep(0.10)

        ser.dtr = False
        ser.rts = False
        time.sleep(0.25)
    finally:
        ser.close()


def patch_uploaderflags(flags, upload_port, env):
    flat = env.Flatten(flags)
    out = []
    i = 0
    saw_port = False
    saw_before = False

    while i < len(flat):
        tok = env.subst(str(flat[i]))

        if tok in ("--port", "-p") and i + 1 < len(flat):
            out.extend([tok, upload_port])
            i += 2
            saw_port = True
            continue

        if tok == "--before" and i + 1 < len(flat):
            out.extend(["--before", "no_reset"])
            i += 2
            saw_before = True
            continue

        out.append(tok)
        i += 1

    if not saw_port:
        out.extend(["--port", upload_port])

    if not saw_before:
        out.extend(["--before", "no_reset"])

    return out


def before_upload(source, target, env, **kwargs):
    prefix = detect_prefix()
    initial_port = prefix + INITIAL_SUFFIX
    reset_port = prefix + RESET_SUFFIX

    print("Detected port prefix:", prefix)
    print("Initial port:", initial_port)
    print("Reset port:", reset_port)

    if not Path(initial_port).exists():
        print("Initial port not found:", initial_port)
        return

    force_bootloader_reset(initial_port)

    if not wait_for_port(reset_port, 10.0):
        print("Reset/upload port not found:", reset_port)
        return

    print("Using upload port:", reset_port)

    env.Replace(UPLOAD_PORT=reset_port)

    current_flags = env.get("UPLOADERFLAGS", [])
    new_flags = patch_uploaderflags(current_flags, reset_port, env)
    env.Replace(UPLOADERFLAGS=new_flags)

    monitor_port = prefix + INITIAL_SUFFIX
    env.Replace(MONITOR_PORT=monitor_port)
    print("Using monitor port:", monitor_port)


env.AddPreAction("upload", before_upload)