"""Check whether Windows exposes any usable Bluetooth radios (not just PnP nodes)."""
import ctypes
import json
from ctypes import wintypes


class FindParams(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD)]


api = ctypes.WinDLL("bthprops.cpl", use_last_error=True)
api.BluetoothFindFirstRadio.argtypes = [ctypes.POINTER(FindParams), ctypes.POINTER(wintypes.HANDLE)]
api.BluetoothFindFirstRadio.restype = wintypes.HANDLE
api.BluetoothFindRadioClose.argtypes = [wintypes.HANDLE]
api.BluetoothFindRadioClose.restype = wintypes.BOOL
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
kernel.CloseHandle.restype = wintypes.BOOL
params = FindParams(ctypes.sizeof(FindParams))
radio = wintypes.HANDLE()
search = api.BluetoothFindFirstRadio(ctypes.byref(params), ctypes.byref(radio))
error = ctypes.get_last_error() if not search else 0
print(json.dumps({"usable_radio_found": bool(search), "win32_error": error,
                  "error_message": ctypes.FormatError(error) if error else None}, indent=2))
if search:
    kernel.CloseHandle(radio)
    api.BluetoothFindRadioClose(search)
