# ESP32-S3 Setup Guide

## Virtual Environment

A Python virtual environment is set up at:
```
/Users/sahrish/projects/esp32-s3-usb-msc/venv/
```

## Quick Start

### Activate Virtual Environment

```bash
cd /Users/sahrish/projects/esp32-s3-usb-msc
source venv/bin/activate
```

You'll see `(venv)` in your prompt when activated.

### Run Scripts

Once activated, you can run any script:

```bash
# Upload a file
python scripts/upload_file.py mydata.txt

# Run workflow test
python scripts/test_workflow.py --mount-path /Volumes/TEST

# Run negative tests
python scripts/test_negative_msc_while_http.py --mount-path /Volumes/TEST
python scripts/test_negative_http_while_msc.py --mount-path /Volumes/TEST
```

### Deactivate

```bash
deactivate
```

## Requirements

Installed packages:
- `pyserial==3.5` - Serial communication
- `requests==2.32.5` - HTTP uploads

Install more packages:
```bash
source venv/bin/activate
pip install <package-name>
pip freeze > requirements.txt
```

## PlatformIO

PlatformIO is installed globally (not in venv):
```bash
which pio
# Output: /opt/homebrew/bin/pio
```

Use PlatformIO normally - it doesn't need the venv.
