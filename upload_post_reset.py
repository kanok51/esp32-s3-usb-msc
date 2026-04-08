from pathlib import Path
import os
import time

Import("env")

try:
    import serial
except ImportError:
    print("pyserial is required for upload_post_reset.py")
    raise

INITIAL_SUFFIX = os.environ.get("ESP_INITIAL_SUFFIX", "usbmodem53140032081")
POST_RESET_MODE = os.environ.get("ESP_POST_RESET_MODE", "b")


def detect_prefix():
    env_prefix = os.environ.get("ESP_PORT_PREFIX")
    if env_prefix in ("/dev/cu.", "/dev/tty."):
        return env_prefix

    for prefix in ("/dev/cu.", "/dev/tty."):
        if Path(prefix + INITIAL_SUFFIX).exists():
            return prefix

    return "/dev/tty."


def force_reset(port, mode="b"):
    sequences = {
        "a": [
            (False, True,  0.10),
            (True,  False, 0.10),
            (False, False, 0.25),
        ],
        "b": [
            (True,  False, 0.10),
            (False, True,  0.10),
            (False, False, 0.25),
        ],
        "c": [
            (False, True,  0.20),
            (True,  False, 0.20),
            (False, False, 0.40),
        ],
        "d": [
            (True,  True,  0.10),
            (False, False, 0.30),
        ],
    }

    ser = serial.Serial(port, 115200)
    try:
        for dtr, rts, delay_s in sequences[mode]:
            ser.dtr = dtr
            ser.rts = rts
            time.sleep(delay_s)
    finally:
        ser.close()


def after_upload(source, target, env, **kwargs):
    port = env.get("ESP_POST_RESET_PORT")
    if not port:
        port = detect_prefix() + INITIAL_SUFFIX

    if not Path(port).exists():
        print("Post-reset port not found:", port)
        return

    try:
        force_reset(port, POST_RESET_MODE)
        print(f"Post-upload reset ({POST_RESET_MODE}) sent on {port}")
    except Exception as e:
        print("Post-upload reset failed on", port, "-", e)


env.AddPostAction("upload", after_upload)