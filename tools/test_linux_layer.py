#!/usr/bin/env python3
"""Exercise the actual native Linux layer, not only portable CPU math."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
shaders = build / 'shaders'
library = build / 'libnative-aa.so'
fixture = build / 'native_scene_layer_tests'
manifest = {'file_format_version': '1.2.0', 'layer': {
    'name': 'VK_LAYER_NATIVE_AA_live_test', 'type': 'GLOBAL',
    'library_path': str(library), 'api_version': '1.3.0',
    'implementation_version': '2', 'description': 'Native AA Linux injected fixture',
    'functions': {'vkNegotiateLoaderLayerInterfaceVersion': 'aaNegotiate'},
    'disable_environment': {'NATIVE_AA_DISABLE': '1'}}}
(build / 'native-aa-test.json').write_text(json.dumps(manifest))
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('NATIVE_AA_', 'XPFSR24_', 'DISABLE_XPFSR24'))}
env.update(VK_LAYER_PATH=str(build) + ':/usr/share/vulkan/explicit_layer.d',
           VK_LOADER_LAYERS_DISABLE='~implicit~',
           VK_INSTANCE_LAYERS='VK_LAYER_NATIVE_AA_live_test',
           NATIVE_AA_EXE=str(fixture), NATIVE_AA_LIVE_TEST='1',
           DISABLE_XPFSR24='1', NATIVE_AA_SHADERS=str(shaders),
           NATIVE_AA_CONTROL=str(build / 'fixture-control.txt'),
           NATIVE_AA_TEST_SHADER=hashlib.sha256((shaders / 'native-scene.frag.spv').read_bytes()).hexdigest(),
           NATIVE_AA_TEST_TAA_SHADER=hashlib.sha256((shaders / 'native-taa-post.frag.spv').read_bytes()).hexdigest())
for mode in ('normal', 'allocation', 'missing-shaders'):
    run_env = env.copy()
    log = build / ('linux-layer-' + mode + '.log')
    log.write_text('')
    run_env['NATIVE_AA_LOG'] = str(log)
    if mode == 'allocation':
        run_env['NATIVE_AA_TEST_FAIL_ALLOC'] = '1'
    if mode == 'missing-shaders':
        run_env['NATIVE_AA_SHADERS'] = str(build / 'missing-shaders')
    subprocess.run([str(fixture), str(shaders), '--allow-single-queue'] + ([] if mode == 'normal' else ['--expect-bypass']),
                   env=run_env, check=True, timeout=480)
    text = log.read_text()
    assert 'scratch_remaining=0' in text, mode + ': incomplete resource retirement'
    if mode == 'normal':
        assert 'COMPLETED aa_frames=' in text and 'taa_frames=' in text, 'Missing GPU completion evidence'
    elif mode == 'allocation':
        assert 'test allocation refusal' in text, 'Allocation fault not exercised'
receipt = {'status': 'PASS', 'platform': 'Linux-x86_64',
           'layerSHA256': hashlib.sha256(library.read_bytes()).hexdigest(),
           'fixtureSHA256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
           'scope': 'Injected synthetic scenes, GPU readback/state/ownership/bypass; single-family software-driver semaphore coverage; not a Linux cross-family or X-Plane flight test'}
(build / 'linux-layer-test-receipt.json').write_text(json.dumps(receipt, indent=2))
print('Native Linux injected SMAA/FXAA/TAA, allocation/shader bypass and zero-scratch retirement PASS')
