#!/usr/bin/env python3
"""Allowlisted Linux beta packaging; requires the actual layer test receipt."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import struct
import tarfile

repo = Path(__file__).resolve().parent.parent
build = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
name = 'Make-XP-AA-0.3.2-linux-beta.1-Linux-x86_64'
stage = output / name
stage.mkdir(parents=True, exist_ok=False)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
receipt = json.loads((build / 'linux-layer-test-receipt.json').read_text())
assert receipt['status'] == 'PASS' and receipt['layerSHA256'] == sha(build / 'libnative-aa.so')
for binary in ('libnative-aa.so', 'lin.xpl'):
    header = (build / binary).read_bytes()[:20]
    assert header[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<H', header, 18)[0] == 62, 'Linux x86_64 ELF required'
library = 'native-aa-' + receipt['layerSHA256'][:12] + '.so'
files = {
    'install-linux.py': repo / 'tools/install-linux.py',
    'README-FIRST.txt': repo / 'docs/LINUX-BETA-INSTALLATION.txt',
    'NativeAA/64/lin.xpl': build / 'lin.xpl',
    'NativeAA/renderer/' + library: build / 'libnative-aa.so',
    'NativeAA/README.txt': repo / 'docs/LINUX-BETA-INSTALLATION.txt',
    'NativeAA/notices/NativeAA-MIT.txt': repo / 'LICENSE',
    'NativeAA/notices/SMAA-MIT.txt': repo / 'third_party/smaa/LICENSE.txt',
    'NativeAA/notices/FXAA-source-and-BSD-notice.txt': repo / 'third_party/fxaa/Fxaa3_11.h',
    'NativeAA/notices/FXAA-Intel-Apache.txt': repo / 'third_party/fxaa/CMAA2-license.txt',
    'NativeAA/notices/XPLM-SDK-license.txt': repo / 'third_party/XPLM-SDK-license.txt',
    'docs/MOTION-SHIMMER-COMBINATIONS.md': repo / 'docs/MOTION-SHIMMER-COMBINATIONS.md',
    'extras/shadows/make XP shadows great again - enhanced edition.lua': repo / 'extras/shadows/make XP shadows great again - enhanced edition.lua',
    'extras/shadows/README.txt': repo / 'extras/shadows/README.txt',
    'docs/CONTROLS.md': repo / 'docs/CONTROLS.md',
    'docs/RELEASE-0.3.2.md': repo / 'docs/RELEASE-0.3.2.md',
    'docs/RENDERER-SELECTION.md': repo / 'docs/RENDERER-SELECTION.md',
    'validation/linux-layer-test-receipt.json': build / 'linux-layer-test-receipt.json',
}
shaders = [p for p in (build / 'shaders').iterdir() if re.fullmatch(r'(live-smaa-[0-3]-[0-2]|fxaa-[02]|native-taa|native-depth-(1|ms))\.comp\.spv', p.name)]
assert len(shaders) == 17
for shader in shaders:
    files['NativeAA/renderer/shaders/' + shader.name] = shader
for relative, source in files.items():
    assert source.is_file() and not source.is_symlink()
    target = stage / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    target.chmod(0o755 if target.suffix in ('.so', '.xpl', '.py') else 0o644)
(stage / 'install.sh').write_text('#!/bin/sh\nexec python3 "$(dirname "$0")/install-linux.py" "$@"\n')
(stage / 'install.sh').chmod(0o755)
source_commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
manifest = dict(product='Make XP Anti-Aliasing Great Again', version='0.3.2-linux-beta.1',
                platform='Linux-x86_64', sourceCommit=source_commit,
                layerSHA256=receipt['layerSHA256'], pluginSHA256=sha(build / 'lin.xpl'),
                files=[dict(path=str(p.relative_to(stage)), sha256=sha(p)) for p in sorted(stage.rglob('*')) if p.is_file()])
(stage / 'RELEASE-MANIFEST.json').write_text(json.dumps(manifest, indent=2))
archive = output / (name + '.tar.gz')
with tarfile.open(archive, 'w:gz') as tar:
    tar.add(stage, arcname=name)
(output / 'SHA256SUMS.txt').write_text(sha(archive) + '  ' + archive.name + '\n')
print('Allowlisted native Linux beta:', archive)
print('Source:', source_commit, 'Layer:', receipt['layerSHA256'])
