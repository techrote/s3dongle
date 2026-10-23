# Upstream provenance

Vendored directory (relative to `BLE/`): `firmware/components/tinyusb_dongle`

- Repository: https://github.com/espressif/esp-iot-solution
- Commit: 4962410c80a51cd5822da1d8d2041eb4285265f1
- Path: examples/usb/device/usb_dongle/components/tinyusb_dongle
- License: Apache-2.0, retained in the component and repository root
- Registry dependency: espressif/tinyusb 0.21.0~2 (MIT; downloaded during build)

Local changes:

- Standalone Bluetooth device descriptor uses class/subclass/protocol E0/01/01,
  allowing whole-device Bluetooth class matching. The interfaces retain the upstream BTH descriptors.
- Application starts the controller before USB enumeration, checks controller status,
  and omits the example's Wi-Fi, CLI, networking, and CDC application code.

The BTH bridge itself initially follows Espressif's example. It is experimental firmware;
USB enumeration alone does not establish pairing, reconnect, suspend, or sustained ACL reliability.
