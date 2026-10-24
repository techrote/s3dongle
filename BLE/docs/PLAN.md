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

## WIP Windows follow-up — 24 October 2026

Per the user's instruction, this iteration builds and tests offline only; no flashing.

1. Inspect USB descriptors and HCI transport against primary specifications.
2. Correct interface count, default ISO bandwidth, historical HCI request support,
   and transport blocking/buffer ownership defects.
3. Run host transport regression tests and inspect the actual built ELF/configuration.
4. Package a separately named WIP image and record limitations and a later smoke test
   that does not require BLE peripherals.
5. Push to a WIP branch with a draft PR. Windows Code 10 resolution remains unverified.

Findings, results, and next test steps: [WINDOWS_CODE10_WIP.md](WINDOWS_CODE10_WIP.md).
