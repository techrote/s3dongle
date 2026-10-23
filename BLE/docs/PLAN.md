# Implementation plan

1. Identify COM13 and preserve its existing flash before replacing firmware.
2. Build a BLE-only USB Bluetooth HCI controller using Espressif's USB dongle component.
3. Use only Bluetooth USB interfaces, with BTH at interface zero. Disable networking and CDC.
4. Flash and verify enumeration, Windows driver status, and Bluetooth host operation.
5. Record exact results, limitations, recovery steps, and reproducible build instructions.

Hardware constraint: ESP32-S3 supports BLE only. Classic Bluetooth cannot be added in software.

Source: espressif/esp-iot-solution, commit 4962410c80a51cd5822da1d8d2041eb4285265f1,
examples/usb/device/usb_dongle/components/tinyusb_dongle (Apache-2.0).
Initial toolchain: existing ESP-IDF v5.5.5 installation.
