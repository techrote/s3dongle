# Validation — 23 October 2026

## Result

Firmware built and flashed successfully. The board enumerates as a USB Bluetooth
device and Windows binds Microsoft's BTHUSB driver. **It is not a working native
Windows Bluetooth adapter:** the driver fails to start (Code 10), and the Windows
Bluetooth API exposes no usable radio. Linux operation, scanning, pairing, GATT,
ACL throughput, suspend/resume, and reconnect are unverified.

The user requested a USB Bluetooth dongle. That end-to-end outcome has not been
achieved on this Windows host. ESP32-S3 is BLE-only; Microsoft's documented Windows
radio requirements include both Basic Rate and LE. This is a material compatibility
constraint, but the exact reason for this particular driver startup failure has not
been isolated by a USB/HCI trace. Do not claim Code 10 alone proves its cause.

## Hardware identified

- Original port: COM13, native USB Serial/JTAG (`303A:1001`), service usbser.
- ESP32-S3 QFN56, revision v0.2, 240 MHz, 40 MHz crystal.
- Embedded flash: 4 MB XMC. Embedded PSRAM: 2 MB (unused by this firmware).
- Base MAC: `1c:29:04:be:cc:48`.
- Full original flash preserved locally: `backups/original-4mb.bin`, 4,194,304 bytes.
- Backup SHA-256: `0dc8e79f4bbd9837d8f17e9e6050eed696530921d1fd215ab24a59458215decc`.

## Build and flash

- ESP-IDF v5.5.5, Xtensa GCC 14.2.0; registry TinyUSB 0.21.0~2.
- Build passed, including application and bootloader partition size checks.
- Application: 407,136 bytes (`0x63660`), fits 1 MB app partition with 61% free.
- Application SHA-256: `b1e213ea2af0b15d0a8a7631e151ff242ff9d27b0dc08351ef1eeef2f83a81dc`.
- Flashed bootloader at 0x0, partition table at 0x8000, app at 0x10000.
- esptool verified hashes for all written regions before resetting.
- Evidence: `evidence/build.log`, `evidence/flash.log`, `evidence/backup.log`.

## Windows checks

- Product string: `S3 BLE USB Dongle`.
- Device: `USB\VID_303A&PID_4080\1C2904BECC48`.
- Class/subclass/protocol: E0/01/01.
- Driver: Generic Bluetooth Adapter, Microsoft BTHUSB, bth.inf,
  version 10.0.26100.9711.
- PnP: Error, problem code 10 (`CM_PROB_FAILED_START`), status `0xC000009E`.
- `BluetoothFindFirstRadio`: no radio, Win32 error 259.
- Evidence: `evidence/windows-pnp.json`, `evidence/windows-radio.json`.
- Attempted PnP restart was denied by Windows permissions. No driver rebinding,
  driver replacement, or operating system installation was performed.
- WSL is not installed. No Linux host was available for testing.

COM13 disappears when the application takes over the native USB PHY. Enter ROM
download mode using BOOT/RESET to flash or restore (see README).

## Next validation on a Linux machine

1. Attach the native USB port and verify `303a:4080` with `lsusb`.
2. Verify `btusb` creates `hciX`; inspect `dmesg` and `btmgmt info`.
3. Power the S3 controller on and perform BLE scanning with `bluetoothctl`.
4. Pair/connect a known BLE peripheral; verify discovery and GATT read/write.
5. Test unplug/replug, controller reset, suspend/resume, and sustained ACL traffic.

For a conventional adapter usable in Windows Settings, use a USB controller with
BR/EDR and LE support. ESP32-S3 cannot acquire BR/EDR capability through firmware.

References:
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/overview.html
- https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/bluetooth
