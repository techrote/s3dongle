# S3 BLE USB Dongle — WIP Windows POC

**WIP, 24 October 2026:** this source contains candidate fixes for the Windows Code 10
startup failure. It has not been flashed or tested on Windows. A successful build or
host test does not establish Windows compatibility. The last hardware-tested firmware
(23 October) still failed with Code 10. Linux operation is also unverified.

See the [Code 10 investigation and test plan](docs/WINDOWS_CODE10_WIP.md).

ESP32-S3 firmware exposing its built-in BLE controller through standard USB Bluetooth HCI
(TinyUSB BTH). The PC runs the Bluetooth host stack. Native USB uses GPIO19 (D-) and
GPIO20 (D+); this project's connected board initially appeared on COM13.

**BLE only:** ESP32-S3 has no Classic Bluetooth radio. This cannot provide A2DP audio,
Classic HID, or RFCOMM/SPP. Bluetooth 5 LE on this chip does not imply LE Audio support.
Windows' documented radio requirements include BR/EDR and LE, so Windows Settings
compatibility must not be assumed. See [validation](docs/VALIDATION.md) for actual results.

## Build

Run the commands below from the `BLE` folder (`cd BLE` from the repository root).
After moving from the original repository layout, build again in this folder;
the old root-level build cache contains absolute paths and should not be reused.

Requires ESP-IDF v5.5.5, with the ESP32-S3 tools installed. The TinyUSB dependency is
pinned by `firmware/dependencies.lock`. In an activated ESP-IDF shell:

```powershell
idf.py -C firmware build
```

On the development PC, `./tools/build.ps1` activates the existing installation.
`./tools/flash.ps1 -Port COM13` uses Python esptool and the generated flash arguments.
After flashing, the COM port is expected to disappear because USB now exposes Bluetooth.

The old, hardware-tested build is saved as `evidence/s3dongle-merged.bin`.
**That image has the original Code 10 failure and does not contain the WIP fixes.**
The candidate image is `evidence/s3dongle-wip-windows-2026-10-24.bin`; it has never
been flashed. See the investigation for the separate manifests and test results.
The following command is only a recovery/reference example for the old image:

```powershell
python -m pip install esptool
python -m esptool --chip esp32s3 --port COM13 write-flash 0 evidence/s3dongle-merged.bin
```

Merged image SHA-256:
`f36616837c199f118f261be217a599542005767fc0afe18514daf9aea4f98da1`.

## Tests without hardware

From `BLE/`:

```powershell
./tests/run-host-tests.ps1
# In the activated ESP-IDF environment, after building:
python tests/check-built-descriptors.py
```

Host tests compile the actual transport C code against deterministic USB/FreeRTOS
mocks. The second check reads the actual target ELF descriptors and preprocesses
TinyUSB with its build flags. Neither test opens COM13 or any USB device.

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
