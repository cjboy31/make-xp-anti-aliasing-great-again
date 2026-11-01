# Offscreen SMAA 1x milestone

This is a native-resolution SDR/sRGB Vulkan test, **not an X-Plane plugin or
installer**. No simulator setting, driver registration, controller, camera or
other add-on is changed. No FSR, motion-vector or neural runtime is used.

## What runs

The original Vulkan wrapper calls the pinned MIT SMAA reference in three passes:
luma edge detection into RG8, blend-weight calculation into RGBA8, and
neighborhood blending into an sRGB attachment. A fullscreen triangle is used.
Edge detection reads encoded UNORM color; neighborhood sampling uses an sRGB
view and the output attachment encodes linear blending back to sRGB. Scene alpha
is restored from the original pixel. Edge and weight targets clear every frame,
including pixels where edge detection discards. Temporal/history modes are off.

`SmaaPipeline::record_pass` records commands only. It owns pipeline/descriptors
and framebuffers; the caller owns images, layouts and synchronization. Construct
with stable, compatible views, fence every use before reuse/destruction, and
recreate on size changes. It is an experimental recorder, not an untrusted-handle
validator or a complete simulator resource manager.

## Reproduce on Linux

On Ubuntu 24.04, install CMake, g++, Vulkan development headers/loader,
glslang-tools, Mesa Vulkan and vulkan-validationlayers. Then:

```sh
python3 tools/verify_vendor.py
cmake -S . -B build-vulkan -DNATIVE_AA_BUILD_VULKAN_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-vulkan --parallel 2
ctest --test-dir build-vulkan --output-on-failure
python3 tools/render_evidence.py build-vulkan/gpu-evidence
```

CI pins Lavapipe via its installed ICD manifest for reproducible software-GPU
checks. For an isolated fixture process, `VK_LOADER_LAYERS_DISABLE='~implicit~'`
excludes unrelated implicit layers. This is never written to system or simulator
configuration. Standard and synchronization validation are required by default;
missing validation is a test failure. A manual build can explicitly opt out with
`-DNATIVE_AA_GPU_VALIDATION=OFF`; its report must not be called validation-passed.

## Reproduce on Windows

With an existing LLVM-MinGW toolchain, official Vulkan headers, glslang and the
installed system Vulkan loader:

```powershell
.\Build-VulkanChecks.ps1 -Compiler <clang++.exe> -VulkanHeaders <headers-root> -Glslang <glslang.exe>
$env:VK_LAYER_PATH = '<official validation-layer directory>'
$env:VK_LOADER_LAYERS_DISABLE = '~implicit~'
.\build\vulkan\smaa_vulkan_tests.exe --shaders build/vulkan/shaders --output build/vulkan/evidence --validation
python tools/render_evidence.py build/vulkan/evidence
```

Use a dedicated test shell so these process-local environment choices do not
affect an unrelated simulator launch. Build products and evidence stay ignored.
No vendor runtime is copied into the repository or installed into X-Plane.

## Evidence and limits

The 2026-10-31 local R9700 run passed 61 rendered frames across 256x144, 63x47
and 257x145 sizes. All 45 diagonal/phase cases improved against independently
generated 16x16 spatial coverage references. Aggregate linear-RGB error ratio
was 0.106796 versus aliased input. This narrow synthetic result is not a claim
about simulator quality, temporal shimmer, cloud detail or FPS.

Checks cover source preservation, exact alpha, deterministic repeat frames,
flat-color roundtrip, low-contrast checker/gradient detail, scratch clearing,
bounded owned allocations and complete fixture resource retirement. Local
standard/synchronization validation reported zero warnings and errors. The peak
image/staging reservation was 1,163,264 bytes for these small fixtures; driver,
pipeline and cache memory is excluded. It is not a full-resolution VRAM estimate.

`result.json` resets at each run and records RUNNING, PASS or FAIL. The image
preview tool refuses an incomplete/failed report, preventing stale readbacks from
being shown as a new successful result. Missing-shader failure was also checked.
CSV metrics and original PPM/PNG images remain in the ignored evidence folder;
the CI artifact contains these synthetic images and validation logs.

No native X-Plane scene boundary is integrated yet. The existing FSR project's
replacement path intercepts a recognized FSR dispatch, which cannot establish an
independent AA path with simulator FSR disabled. A new verified color/resolve/UI
boundary is required. Do not force FSR on or guess an image by size to bridge it.
HDR transfer functions, MSAA resolve, real aircraft/texture/cloud motion, VR,
multiple windows and macOS Metal remain separate integration/validation gates.
