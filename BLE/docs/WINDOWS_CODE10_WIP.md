# WIP: Windows Code 10 investigation — 24 October 2026

**Unflashed proof of concept. Windows startup success is not established.**
All new work is under `BLE/`. No board, USB driver, or COM port was changed in this
iteration. The old firmware's PnP and radio evidence remains historical evidence.

## What the original failure establishes

Windows enumerated `303A:4080`, matched `bth.inf`, and loaded BTHUSB, then reported
Code 10 / `0xC000009E`. Its Bluetooth radio API exposed no usable radio. Thus USB
enumeration and driver matching worked; driver/controller initialization did not.
There is no captured USB/HCI trace showing the first failed request. Code 10 does
not identify the failed opcode or prove that BLE-only capability is the cause.

## Findings and changes

| Finding in the old source | Candidate correction | Confidence and remaining uncertainty |
| --- | --- | --- |
| Configuration `bNumInterfaces` was 1, but the BTH descriptor/IAD described interfaces 0 and 1. Confirmed in compiled ELF bytes: `09 02 46 00 01 ...`. | Count the voice interface in the enum, so the configuration reports 2 interfaces. | Concrete USB configuration mismatch and a strong startup-failure candidate. Still requires a Windows retest. The new ELF test caught this in the first candidate build. |
| `CFG_TUD_BTH_ISO_ALT_COUNT=1` expanded a 64-byte voice endpoint pair at alternate 0. The additional macro argument did not create another alternate. | Pass `0` as the sole voice size. Keep only alternate 0, with zero bandwidth. | Concrete descriptor defect. A plausible startup blocker, not a traced cause of this Code 10. |
| TinyUSB historical device-addressed HCI requests were disabled. | Set `CFG_TUD_BTH_HISTORICAL_COMPATIBLE=1`. | Concrete compatibility gap. No trace proves this Windows version sent request 0xE0. |
| VHCI callback waited for USB completion; USB command callback waited for controller availability. | Copy packets to bounded queues; separate USB and controller workers. | Removes potentially interdependent blocking waits during startup. Original deadlock was not captured on hardware. |
| USB send return values were ignored before waiting forever on semaphores. | Retry failed/busy sends; wait for completion only after a transfer starts. | Concrete transport error path fixed. |
| ACL header used an unaligned integer load and unbounded fragment copies. | Byte-wise length parsing, packet size limits, overflow checks, copied ownership. | Transport correctness fix; not evidence of the original startup cause. |

The Bluetooth USB transport specification describes zero bandwidth at default voice
setting and historical request compatibility. Microsoft's USB guidance also requires
zero default isochronous bandwidth. TinyUSB documents send-buffer ownership and its
historical-request option. Sources:

- [Bluetooth Core, USB transport sections 2.1–2.2](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core_v6.3/out/en/host-controller-interface/usb-transport-layer.html)
- [Microsoft USB bandwidth allocation](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-bandwidth-allocation)
- [TinyUSB Bluetooth HCI](https://docs.tinyusb.org/en/latest/reference/class/bluetooth.html)

## POC scope and deliberate limits

The product string is `S3 BLE Dongle WIP POC`, firmware version 0.2.0, bcdDevice
0x0101; VID/PID and serial stay the same for this development board.
HCI commands and capability responses pass through unchanged. There is no simulated
BR/EDR radio, no made-up command success, and no Windows driver replacement.

ESP32-S3 is BLE-only. [Microsoft's published Bluetooth hardware guidance](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/bluetooth)
requires Basic Rate and LE. Even after correcting USB transport, native Windows may
reject the controller or an unsupported startup command. This POC makes the transport
more correct; it cannot promise to overcome that host policy or add Classic radio support.

Twelve packets fit each direction's queue. A single worker serializes event/ACL IN
transfers, retaining its aligned internal-RAM buffer until completion, including an
ACL zero-length packet. This is intended for bring-up, not throughput optimization.
Queue overflow or a three-second send/completion timeout resets the chip. Consequently
disconnect, slow hosts, or suspend may cause reset/re-enumeration. Reconnect and power
management are not production-ready. Live teardown also requires a chip reset.

UART0 (S3 TX GPIO43, RX GPIO44, 115200 baud) reports command opcode, completion/status,
and transport reset reasons. It does not log keys or HCI payloads. Capturing this
requires an external 3.3 V UART adapter; native USB belongs to the Bluetooth function.

## Reproduce the offline checks

Results from this iteration:

- Full ESP-IDF 5.5.5 build passed without compiler warnings. The initial ELF check
  failed on `bNumInterfaces=1`; after the enum correction, the incremental rebuild
  and ELF/preprocessor test passed. The old hardware-tested ELF also reports 1.
- Host Clang build passed with `-Wall -Wextra -Werror`; transport tests passed.
- Final application size is 408,432 bytes (`0x63B70`), fitting the 1 MB partition.
- Unflashed merged image: `evidence/s3dongle-wip-windows-2026-10-24.bin`, 473,968 bytes.
- Image SHA-256: `0a1b641e82de8df181dc1e274b3e4bf78ad67c70e232c72e1c0f3d350dde173a`.
- [Artifact manifest](../evidence/s3dongle-wip-windows-2026-10-24.json) records image,
  application, configuration, and source hashes, with explicit unverified status.
- Original flash image and 23 October hardware evidence were preserved.

From `BLE/`, build with ESP-IDF v5.5.5, then run:

```powershell
./tools/build.ps1
./tests/run-host-tests.ps1
# Use the activated ESP-IDF Python environment for pyelftools:
python tests/check-built-descriptors.py
# Package only, without flashing:
python tools/package-wip.py
```

The host harness compiles the actual `tusb_bth.c`, mocking USB/FreeRTOS/controller APIs.
It checks copied callback ownership, H4 command forwarding, ACL fragments, malformed
lengths, USB busy retry, completion timeout, controller timeout, queue exhaustion, and
DMA alignment. It does not simulate USB scheduling, interrupt timing, or Windows.

The ELF test verifies actual compiled device/configuration descriptors, lengths,
interfaces and endpoint sizes. It preprocesses the actual TinyUSB compile command
to verify historical-request support is enabled. It does not exercise an actual
Windows control transfer. Build/test logs and the artifact manifest live in `evidence/`.

## Later smoke test — no BLE peripheral needed

These are instructions for a future hardware test, **not performed in this iteration**.

1. Enter BOOT/RESET download mode and flash the candidate image manually. Use its
   manifest SHA-256 to distinguish it from the old `s3dongle-merged.bin`.
2. Verify the WIP product string / bcdDevice 0x0101 in a USB descriptor viewer.
3. Run `./tools/inspect-device.ps1`. For the S3 serial, require BTHUSB service,
   `Status=OK`, and `ProblemCode=0`. Enumeration alone is not a pass.
4. Run `python tools/check-windows-radio.py`. Require a usable radio. If the PC has
   other adapters, that script's first-radio result alone cannot establish S3 success;
   identify the S3 radio separately or test with no other active adapters.
5. Confirm the Bluetooth toggle/radio appears in Windows. An empty discovery list
   is expected if no BLE peripherals are nearby; it does not imply initialization failure.
6. Capture UART0 opcode/status logs and a USBPcap/Wireshark trace if Code 10 persists.
   Record the first stalled control request, missing completion, or nonzero HCI status.
   A completion for Reset (0x0C03) followed by local capability/version queries is
   useful progress, not a guarantee that the entire Windows initialization completed.

No-peripheral acceptance is limited to driver start and local radio enumeration.
Scanning reception, pairing, GATT read/write, sustained ACL traffic, suspend/resume,
and reconnect still require separate validation. Do not mark this WIP as working
until those results exist.
