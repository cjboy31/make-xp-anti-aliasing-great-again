#!/usr/bin/env python3
"""Native AA Linux beta installer. Standard library only; never requires sudo."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid

VERSION = '0.3.0-linux-beta.1'
PACKAGE = Path(__file__).resolve().parent


def sha(path):
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else hashlib.sha256(file.read()).hexdigest()


def within(root, relative):
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise ValueError('Invalid owned-file path')
    target = root / path
    for check in [target, *target.parents]:
        if check == root.parent:
            break
        if check.is_symlink():
            raise ValueError('Refusing redirected Native AA path: ' + str(check))
    if not target.resolve().is_relative_to(root):
        raise ValueError('Owned path escaped NativeAA')
    return target


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.nativeaa-' + uuid.uuid4().hex)
    try:
        with temporary.open('xb') as file:
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def registration_path(root):
    base = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
    if not base.is_absolute():
        raise ValueError('XDG_DATA_HOME must be an absolute path')
    base = base.resolve()
    identity = hashlib.sha256(os.fsencode(root)).hexdigest()[:12]
    return base / 'vulkan/implicit_layer.d' / ('native-aa-' + identity + '.json')


def inventory(destination, root):
    path = within(destination, 'installation-linux.json')
    if not path.exists():
        if (destination / 'installation.json').exists():
            raise ValueError('A different-platform Native AA inventory exists. Keep simulator installations separate.')
        return None
    data = json.loads(path.read_text())
    if data.get('product') != 'NativeAA-Linux' or data.get('simulator') != str(root):
        raise ValueError('Installation inventory does not match this simulator')
    for file in data['files']:
        within(destination, file['path'])
        if len(file['sha256']) != 64 or any(c not in '0123456789abcdef' for c in file['sha256']):
            raise ValueError('Invalid owned-file hash')
    registered = data.get('registration')
    if registered and registered['path'] != str(registration_path(root)):
        raise ValueError('Registration belongs to another simulator or XDG location')
    return data


def simulator(selected):
    if os.geteuid() == 0:
        raise ValueError('Run setup as your normal user, without sudo')
    root = Path(selected).expanduser().resolve(strict=True)
    executable = root / 'X-Plane-x86_64'
    if not executable.is_file() or not os.access(executable, os.X_OK) or not (root / 'Resources/plugins').is_dir():
        raise ValueError('Select the root containing executable X-Plane-x86_64 and Resources/plugins')
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            if (process / 'exe').resolve(strict=True) == executable.resolve():
                raise ValueError('Close this X-Plane normally before installing or uninstalling')
        except (OSError, RuntimeError):
            pass
    return root


def install(root, registration=True):
    manifest = json.loads((PACKAGE / 'RELEASE-MANIFEST.json').read_text())
    if manifest.get('version') != VERSION or manifest.get('platform') != 'Linux-x86_64':
        raise ValueError('Incorrect package version/platform')
    for file in manifest['files']:
        source = within(PACKAGE, file['path'])
        if sha(source) != file['sha256']:
            raise ValueError('Package file missing/changed: ' + file['path'])
    destination = root / 'Resources/plugins/NativeAA'
    previous = inventory(destination, root)
    files = [dict(path=f['path'][9:], sha256=f['sha256']) for f in manifest['files'] if f['path'].startswith('NativeAA/')]
    for file in files:
        within(destination, file['path'])
    within(destination, 'standalone.enabled')
    registered = registration_path(root) if registration else None
    if registered and registered.is_symlink():
        raise ValueError('Refusing redirected layer manifest')
    if registered and registered.exists():
        old = previous.get('registration') if previous else None
        if not old or old['path'] != str(registered) or sha(registered) != old['sha256']:
            raise ValueError('Existing layer manifest is not an unchanged owned file; no files changed')
    for file in files:
        target = within(destination, file['path'])
        target.parent.mkdir(parents=True, exist_ok=True)
        write_atomic(target, within(PACKAGE, 'NativeAA/' + file['path']).read_bytes())
    marker = within(destination, 'standalone.enabled')
    write_atomic(marker, b'NativeAAStandalone=1\n' if registration else b'NativeAAStandalone=0\n')
    files.append(dict(path='standalone.enabled', sha256=sha(marker)))
    layer_record = None
    if registered:
        library = next(f['path'] for f in files if f['path'].endswith('.so'))
        body = {'file_format_version': '1.2.0', 'layer': {
            'name': 'VK_LAYER_NATIVE_AA_' + registered.stem.removeprefix('native-aa-'),
            'type': 'GLOBAL', 'library_path': str(within(destination, library)),
            'api_version': '1.3.0', 'implementation_version': '2',
            'description': 'Native AA Linux beta; exact selected simulator only',
            'functions': {'vkNegotiateLoaderLayerInterfaceVersion': 'aaNegotiate'},
            'disable_environment': {'NATIVE_AA_DISABLE': '1'}}}
        write_atomic(registered, json.dumps(body, indent=2).encode())
        layer_record = dict(path=str(registered), sha256=sha(registered))
    owned = {f['path'] for f in files}
    if previous:
        for file in previous['files']:
            old = within(destination, file['path'])
            if file['path'] not in owned and old.is_file() and sha(old) == file['sha256']:
                old.unlink()
    data = dict(product='NativeAA-Linux', version=VERSION, simulator=str(root), files=files, registration=layer_record)
    write_atomic(within(destination, 'installation-linux.json'), json.dumps(data, indent=2).encode())
    print('Installed Native AA Linux beta. Start normally, then Plugins > Native AA > Open AA controls.')
    print('Use Full render input and check fresh AA ACTIVE/TAA ACTIVE; Linux flight compatibility is still under beta validation.')


def uninstall(root):
    destination = root / 'Resources/plugins/NativeAA'
    previous = inventory(destination, root)
    if not previous:
        raise ValueError('No Native AA Linux installation inventory found; no files removed')
    registered = previous.get('registration')
    if registered:
        path = Path(registered['path'])
        if path.is_symlink():
            raise ValueError('Refusing redirected layer manifest')
        if path.is_file() and sha(path) == registered['sha256']:
            path.unlink()
        elif path.exists():
            print('Preserved edited registration:', path)
    for file in previous['files']:
        path = within(destination, file['path'])
        if path.is_file() and sha(path) == file['sha256']:
            path.unlink()
        elif path.exists():
            print('Preserved user-modified file:', path)
    within(destination, 'installation-linux.json').unlink()
    for relative in ('renderer/shaders', 'renderer', 'notices', '64', ''):
        folder = destination / relative
        if folder.is_dir() and not any(folder.iterdir()):
            folder.rmdir()
    print('Native AA removed. Saved preferences, unknown/modified user files and other layers are retained.')


def choose_folder():
    try:
        import tkinter as tk
        from tkinter import filedialog
        window = tk.Tk()
        window.withdraw()
        try:
            return filedialog.askdirectory(title='Select X-Plane 12 root (X-Plane-x86_64)')
        finally:
            window.destroy()
    except Exception as error:
        if sys.stdin.isatty():
            return input('X-Plane 12 root folder: ').strip()
        raise ValueError('Use --simulator "/path/to/X-Plane 12" from a terminal. Optional python3-tk provides the folder picker.') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator')
    parser.add_argument('--uninstall', action='store_true')
    parser.add_argument('--no-registration', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    selected = args.simulator if args.simulator else choose_folder()
    if not selected:
        print('Cancelled. No changes made.')
        return
    root = simulator(selected)
    if args.uninstall:
        uninstall(root)
    else:
        install(root, not args.no_registration)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('Native AA setup:', error, file=sys.stderr)
        sys.exit(1)
