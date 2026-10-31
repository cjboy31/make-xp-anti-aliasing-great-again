#!/usr/bin/env python3
"""Extracted-package lifecycle plus real loader admission/isolation checks."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

archive, probe = map(lambda value: Path(value).resolve(), sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='native-aa-linux-') as temporary:
    base = Path(temporary)
    with tarfile.open(archive) as tar:
        for member in tar.getmembers():
            assert not member.issym() and not member.islnk()
            assert (base / member.name).resolve().is_relative_to(base)
        tar.extractall(base)
    package = next(base.glob('Make-XP-AA-*'))
    manifest = json.loads((package / 'RELEASE-MANIFEST.json').read_text())
    assert manifest['platform'] == 'Linux-x86_64'
    for file in manifest['files']:
        assert hashlib.sha256((package / file['path']).read_bytes()).hexdigest() == file['sha256']
    payloads = [p for p in package.rglob('*') if p.is_file()]
    assert len(payloads) == len(manifest['files']) + 1
    assert not any(p.suffix.lower() in ('.dll', '.exe', '.rdc', '.onnx', '.mp4') for p in payloads)
    plugin = package / 'NativeAA/64/lin.xpl'
    symbols = subprocess.check_output(['nm', '-D', '--defined-only', str(plugin)], text=True)
    for symbol in ('XPluginStart', 'XPluginStop', 'XPluginEnable', 'XPluginDisable', 'XPluginReceiveMessage'):
        assert symbol in symbols
    simulator = base / 'Simulator with spaces'
    (simulator / 'Resources/plugins').mkdir(parents=True)
    shutil.copy2(probe, simulator / 'X-Plane-x86_64')
    outside = base / 'unrelated-vulkan-app'
    shutil.copy2(probe, outside)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('NATIVE_AA_', 'XPFSR24_', 'DISABLE_XPFSR24', 'VK_LAYER', 'VK_INSTANCE', 'VK_LOADER_LAYERS'))}
    env.update(XDG_DATA_HOME=str(base / 'user-data'), XDG_CONFIG_HOME=str(base / 'user-config'),
               XDG_DATA_DIRS=str(base / 'data-dirs'), XDG_CONFIG_DIRS=str(base / 'config-dirs'))

    def setup(*args, expected=0):
        result = subprocess.run([sys.executable, str(package / 'install-linux.py'), '--simulator', str(simulator), *args], env=env)
        assert result.returncode == expected, ('setup', args, result.returncode, expected)

    def activation(executable, selected, **extra):
        subprocess.run([str(executable), 'selected' if selected else 'passive'], env={**env, **extra}, check=True, timeout=30)

    setup()
    destination = simulator / 'Resources/plugins/NativeAA'
    record_path = destination / 'installation-linux.json'
    record = json.loads(record_path.read_text())
    registration = Path(record['registration']['path'])
    other_manifest = registration.with_name('unrelated.keep')
    other_manifest.write_text('another application owns this')
    activation(outside, False)
    activation(simulator / 'X-Plane-x86_64', True)
    activation(simulator / 'X-Plane-x86_64', False, NATIVE_AA_DISABLE='1')
    marker = destination / 'standalone.enabled'
    marker.write_text('NativeAAStandalone=0\n')
    activation(simulator / 'X-Plane-x86_64', False)
    marker.write_text('NativeAAStandalone=1\n')
    activation(simulator / 'X-Plane-x86_64', False, XPFSR24_RUNTIME='/fixture/selected-fsr-runtime')
    unchanged = (destination / '64/lin.xpl').read_bytes()
    original = record_path.read_text()
    record['files'].append(dict(path='../../outside.txt', sha256='0' * 64))
    record_path.write_text(json.dumps(record))
    setup(expected=1)
    setup('--uninstall', expected=1)
    assert (destination / '64/lin.xpl').read_bytes() == unchanged and registration.exists()
    record_path.write_text(original)
    readme = destination / 'README.txt'
    readme_bytes = readme.read_bytes()
    readme.unlink()
    external = base / 'outside-user-file.txt'
    external.write_text('keep outside file')
    readme.symlink_to(external)
    setup('--uninstall', expected=1)
    assert external.read_text() == 'keep outside file' and registration.exists()
    readme.unlink()
    readme.write_bytes(readme_bytes)
    old = destination / 'obsolete-owned.so'
    old.write_text('old owned version')
    unknown = destination / 'user-note.txt'
    unknown.write_text('keep user file')
    record = json.loads(record_path.read_text())
    record['files'].append(dict(path=old.name, sha256=hashlib.sha256(old.read_bytes()).hexdigest()))
    record_path.write_text(json.dumps(record))
    setup()
    assert not old.exists() and unknown.read_text() == 'keep user file'
    readme.write_text('edited README notes')
    preferences = simulator / 'Output/preferences/NativeAA.ini'
    preferences.parent.mkdir(parents=True, exist_ok=True)
    preferences.write_text('saved preferences')
    setup('--uninstall')
    assert not registration.exists() and not (destination / '64/lin.xpl').exists()
    assert unknown.read_text() == 'keep user file' and readme.read_text() == 'edited README notes'
    assert preferences.read_text() == 'saved preferences'
    assert other_manifest.read_text() == 'another application owns this'
print('Linux package hashes/exports/lifecycle, path/symlink rejection, five real loader cases and user-data preservation PASS')
