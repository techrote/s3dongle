# S3 BLE USB Dongle

**Current result:** built and flashed on 23 October 2026. Windows detects the USB
Bluetooth device but its native driver fails to start (Code 10). Linux operation
has not been tested. This is experimental firmware, not a verified Windows dongle.

ESP32-S3 firmware exposing its built-in BLE controller through standard USB Bluetooth HCI
(TinyUSB BTH). The PC runs the Bluetooth host stack. Native USB uses GPIO19 (D-) and
GPIO20 (D+); this project's connected board initially appeared on COM13.

**BLE only:** ESP32-S3 has no Classic Bluetooth radio. This cannot provide A2DP audio,
Classic HID, or RFCOMM/SPP. Bluetooth 5 LE on this chip does not imply LE Audio support.
Windows' documented radio requirements include BR/EDR and LE, so Windows Settings
compatibility must not be assumed. See [validation](docs/VALIDATION.md) for actual results.

## Build

Requires ESP-IDF v5.5.5, with the ESP32-S3 tools installed. The TinyUSB dependency is
pinned by `firmware/dependencies.lock`. In an activated ESP-IDF shell:

```powershell
idf.py -C firmware build
idf.py -C firmware -p COM13 flash
```

On the development PC, `./tools/build.ps1` activates the existing installation.
`./tools/flash.ps1 -Port COM13` uses Python esptool and the generated flash arguments.
After flashing, the COM port is expected to disappear because USB now exposes Bluetooth.

The exact build tested is also saved as `evidence/s3dongle-merged.bin`. From ROM
download mode it can be flashed without ESP-IDF:

```powershell
python -m pip install esptool
python -m esptool --chip esp32s3 --port COM13 write-flash 0 evidence/s3dongle-merged.bin
```

Merged image SHA-256:
`f36616837c199f118f261be217a599542005767fc0afe18514daf9aea4f98da1`.

## Recovery and restoring the original firmware

Hold BOOT, press and release RESET, then release BOOT to enter ROM download mode.
If no RESET button exists, hold BOOT while reconnecting USB. Find the new COM number
in Device Manager, then flash again. Native USB Serial/JTAG is available in ROM mode
even when the application uses USB OTG.

The original full flash backup is local only at `backups/original-4mb.bin` and is excluded
from Git because it may contain private configuration. To restore it from ROM download mode:

```powershell
python -m esptool --chip esp32s3 --port COM13 write-flash 0 backups/original-4mb.bin
```

## Linux host

With the device connected directly to Linux and `btusb`/BlueZ installed:

```sh
sudo modprobe btusb
lsusb -d 303a:4080
sudo btmgmt info
bluetoothctl list
bluetoothctl
# select the S3 controller address if multiple adapters exist
# power on
# scan on
```

This is USB BTH, so `btattach` is unnecessary. A Linux VM needs direct USB passthrough.
Linux scanning/pairing results are only verified if explicitly recorded in the validation log.

## Sources and licensing

The vendored `firmware/components/tinyusb_dongle` comes from Espressif's
[USB dongle example](https://github.com/espressif/esp-iot-solution/tree/4962410c80a51cd5822da1d8d2041eb4285265f1/examples/usb/device/usb_dongle).
Its source headers and Apache-2.0 license are retained. Project additions use Apache-2.0.
See [plan](docs/PLAN.md) for the work plan and [validation](docs/VALIDATION.md) for evidence.

References:
- [ESP32-S3 BLE capabilities](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/overview.html)
- [Microsoft Bluetooth hardware requirements](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/bluetooth)
