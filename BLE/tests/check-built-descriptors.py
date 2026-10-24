"""Inspect the real target ELF and TinyUSB preprocessor macros, without hardware.

Run with ESP-IDF's Python after `idf.py build`. No serial/USB device is opened.
"""
import json
import pathlib
import subprocess
import os
from elftools.elf.elffile import ELFFile

project = pathlib.Path(__file__).resolve().parents[1]
build = project / 'firmware' / 'build'
with (build / 's3dongle.elf').open('rb') as stream:
    elf = ELFFile(stream)
    symbols = elf.get_section_by_name('.symtab')
    def read_symbol(name):
        matches = symbols.get_symbol_by_name(name)
        assert matches, f'Missing symbol {name}'
        sym = matches[0]
        section = elf.get_section(sym['st_shndx'])
        start = sym['st_value'] - section['sh_addr']
        return section.data()[start:start + sym['st_size']]

    device = read_symbol('descriptor_dev_default')
    config = read_symbol('descriptor_fs_cfg_default')
    assert device[4:7] == bytes([0xe0, 1, 1]), 'Bluetooth device class'
    assert device[12:14] == bytes([1, 1]), 'WIP bcdDevice 0x0101'
    assert int.from_bytes(config[2:4], 'little') == len(config), 'Total length'
    assert config[4] == 2, 'Exactly two Bluetooth interfaces'
    offset, interface, alternate = 0, None, None
    endpoints = []
    while offset < len(config):
        size, kind = config[offset:offset + 2]
        assert size >= 2 and offset + size <= len(config)
        descriptor = config[offset:offset + size]
        if kind == 4:
            interface, alternate = descriptor[2:4]
            assert descriptor[5:8] == bytes([0xe0, 1, 1])
            assert alternate == 0, 'BLE POC advertises no SCO alternates'
        elif kind == 5:
            endpoint = (interface, alternate, descriptor[2], descriptor[3] & 3,
                        int.from_bytes(descriptor[4:6], 'little'))
            endpoints.append(endpoint)
        offset += size
    assert sorted((kind, size) for itf, alt, addr, kind, size in endpoints if itf == 0) == [(2, 64), (2, 64), (3, 16)]
    voice = [(kind, size) for itf, alt, addr, kind, size in endpoints if itf == 1]
    assert voice == [(1, 0), (1, 0)], 'Default voice endpoints must have zero bandwidth'

# Use the compiler's actual TinyUSB command line so this checks the configuration
# compiled into the driver, rather than searching the application header as text.
commands = json.loads((build / 'compile_commands.json').read_text())
command = next(c for c in commands if c['file'].replace('\\', '/').endswith('/class/bth/bth_device.c'))
args = command.get('arguments')
if args is None:
    if os.name == 'nt':
        import ctypes
        count = ctypes.c_int()
        shell = ctypes.WinDLL('shell32')
        shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
        shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
        argv = shell.CommandLineToArgvW(command['command'], ctypes.byref(count))
        assert argv, 'Cannot parse compiler command'
        args = [argv[i] for i in range(count.value)]
        kernel = ctypes.WinDLL('kernel32')
        kernel.LocalFree.argtypes = [ctypes.c_void_p]
        kernel.LocalFree.restype = ctypes.c_void_p
        kernel.LocalFree(argv)
    else:
        import shlex
        args = shlex.split(command['command'])
clean = []
skip = False
for arg in args:
    if skip:
        skip = False
        continue
    if arg in ('-o', '-MF', '-MT'):
        skip = True
    elif arg not in ('-c', '-MD', '-MMD', '-MP'):
        clean.append(arg)
macros = subprocess.check_output(clean + ['-E', '-dM'], cwd=command['directory'], text=True)
assert '#define CFG_TUD_BTH_HISTORICAL_COMPATIBLE 1' in macros
print('PASS: real ELF Bluetooth descriptors, zero-bandwidth ISO alt 0, WIP revision, historical HCI requests enabled in compiled TinyUSB configuration')
