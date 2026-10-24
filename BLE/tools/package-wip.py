"""Package an already built WIP image and checksums. Does not open any device."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name', default='s3dongle-wip-windows-2026-10-24')
args = parser.parse_args()
assert Path(args.name).name == args.name, 'Use a file basename'
project = Path(__file__).resolve().parents[1]
build = project / 'firmware' / 'build'
image = project / 'evidence' / (args.name + '.bin')
subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3', 'merge_bin',
                '-o', str(image), '@flash_args'], cwd=build, check=True)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

description = json.loads((build / 'project_description.json').read_text())
flash = json.loads((build / 'flasher_args.json').read_text())
sources = sorted(p for p in (project / 'firmware').rglob('*') if p.is_file()
                 and not {'build', 'managed_components'}.intersection(p.relative_to(project / 'firmware').parts)
                 and p.name not in {'sdkconfig', 'sdkconfig.old'})
manifest = {
    'status': 'WIP - UNFLASHED - WINDOWS CODE 10 FIX NOT VERIFIED',
    'firmware_version': description['project_version'],
    'idf_version': description['git_revision'],
    'hardware_tested': False,
    'windows_startup_verified': False,
    'ble_peripheral_tested': False,
    'image': image.name,
    'flash_offset': '0x0',
    'image_bytes': image.stat().st_size,
    'image_sha256': digest(image),
    'application_sha256': digest(build / 's3dongle.bin'),
    'configured_sdkconfig_sha256': digest(project / 'firmware' / 'sdkconfig'),
    'flash_settings': flash['flash_settings'],
    'source_sha256': {str(p.relative_to(project)).replace('\\', '/'): digest(p) for p in sources},
    'test_evidence': ['wip-host-tests-2026-10-24.log', 'wip-descriptor-tests-2026-10-24.log',
                      'wip-build-2026-10-24.log', 'wip-rebuild-2026-10-24.log'],
}
manifest_path = image.with_suffix('.json')
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(f'WIP package: {image.name}, SHA-256 {manifest["image_sha256"]}')
print(f'Manifest: {manifest_path.name}; no device was opened')
