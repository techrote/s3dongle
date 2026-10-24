# Upstream provenance

Vendored directory (relative to `BLE/`): `firmware/components/tinyusb_dongle`

- Repository: https://github.com/espressif/esp-iot-solution
- Commit: 4962410c80a51cd5822da1d8d2041eb4285265f1
- Path: examples/usb/device/usb_dongle/components/tinyusb_dongle
- License: Apache-2.0, retained in the component and `BLE/LICENSE`
- Registry dependency: espressif/tinyusb 0.21.0~2 (MIT; downloaded during build)

Local changes:

- Standalone Bluetooth device descriptor uses class/subclass/protocol E0/01/01,
  allowing whole-device Bluetooth class matching.
- Application starts the controller before USB enumeration, checks controller status,
  and omits the example's Wi-Fi, CLI, networking, and CDC application code.

WIP changes on 24 October 2026:

- Count the voice interface as well as the primary interface in `bNumInterfaces`.
- Default voice ISO endpoints now use zero packet size, with only alternate setting zero.
- `CFG_TUD_BTH_HISTORICAL_COMPATIBLE=1` accepts historical device-addressed HCI requests.
- The BTH bridge copies packets into bounded queues and sends them from worker tasks.
  USB/VHCI callbacks no longer wait indefinitely for the opposite transport.
- USB sends retry busy endpoints; completion is checked before reusing an aligned
  buffer. Transport stalls/queue exhaustion reset the chip instead of silently losing HCI credits.
- ACL parsing uses byte-wise lengths and bounds checks; initialization errors fail fast.
- UART0 reports command opcode/status and faults. WIP product string and USB revision
  identify this candidate; firmware does not fabricate Classic Bluetooth capabilities.

These changes are experimental. USB enumeration alone does not establish pairing,
reconnect, suspend, or sustained ACL reliability. See `WINDOWS_CODE10_WIP.md`.
