# Windows native-scene experiment

Branch: `experimental/live-smaa`, stacked on the temporal research branch.
This is developer source, not an end-user installation package. Windows has
live SMAA/FXAA controls and saved selections. The temporal resolve is tested
offscreen only; verified live motion/jitter inputs remain unfinished. Working
Linux/Metal simulator adapters and general compatibility are not claimed.

## Evidence and admission

A private RenderDoc capture of X-Plane 12.4.4 on an AMD Radeon AI PRO R9700
identified a 3840x2160 `R8G8B8A8_SRGB`, single-sample LDR scene before the
simulator's later offscreen/UI composition. The scene image already had both
transfer usage bits. The native scene was full size; simulator MSAA was 2x,
FXAA was Off, and FSR/NR/frame generation were disabled. Windows desktop HDR
being enabled does not turn this captured SRGB8 surface into PQ/scRGB data.

The private capture and all simulator pixels/assets remain outside Git.
Only shader SHA-256 identifiers are used for the exact-version admission
check; no simulator shader bytes are distributed. An unrecognized shader,
image role, format, rendering pass or native-size contract must bypass AA.
The normal-launch probe first left frames unchanged because it did not recognize
the legacy debug names and X-Plane's single-layer array view with remaining
levels/layers. Those forms are now handled explicitly. A subsequent live run
submitted/completed more than 1,200 native 4K AA frames, then the simulator
closed unexpectedly. That run is **not a successful stability result**.

Review found an ignored-field read in immutable-sampler tracking. A deliberately
unreadable `pImmutableSamplers` in a buffer binding (a field Vulkan ignores for
that descriptor type) reproduced an access violation in the old adapter; the
corrected adapter passes the same regression with zero validation messages.
This proves the adapter bug and correction, not the cause of the simulator
exit, which had no fresh dump. The corrected build completed an instrumented
OFF/ON/OFF/ON stationary-view comparison (12/8/8/35 seconds), more than 2,400
GPU-fence-confirmed AA frames and four native simulator screenshots. Its local
fault log remained empty. The sampled median simulator frame period was
18.57-18.62 ms Off and 18.68-19.01 ms On (about 54 versus 53 FPS), with scratch
remaining 134,095,872 bytes. These are short, filtered dataref samples from one
R9700 scene, not a full frame-time distribution, GPU-time measurement or a
long-flight stability/quality result. Further user panning/cockpit checks remain
in progress; this is still not a rendering release.

## Compute transport

The compute wrapper calls the pinned MIT SMAA implementation with explicit
LOD-zero sampling. Its three passes write every in-bounds pixel, including
zero edges, so no stale discard/weight data survives a reused image. It uses
encoded UNORM luma for edges, an SRGB sampling view for linear-light blending,
explicit SRGB encoding for its UNORM output, and unfiltered original alpha.
The extent is specialized into the pipelines; it never writes push constants.

The adapter copies the admitted scene into private scratch, records the three
compute passes, and copies the encoded result back into the original scene
image before UI composition. It restores the native image layout and publishes
its transfer write to subsequent operations. It restores previously valid
compute descriptors, dynamic buffer offsets and the compute pipeline. It does
not bind a graphics pipeline or change viewport/scissor/vertex/index state.
Unsupported descriptor-buffer/shader-object, push-descriptor, secondary and
other unverified state paths bypass instead of guessing restoration.

No native image usage, device feature or system layer registration is changed.
Scratch is owned by the admitted scene image, reused under the native image's
ordering, restricted to one queue family, and retired when that image is
destroyed. There is a fixed 384 MiB device-local/staging reservation cap across
all cached scenes per logical device. The 4K fixture reserved 134,202,368 bytes (about 128 MiB),
excluding driver and pipeline memory. Allocation/shader failures occur before
AA commands are recorded and retain the original frame. This is not a measured
whole-simulator VRAM total or an out-of-memory guarantee.

The recording path submits no work, waits on no fence and performs no CPU
readback. `RECORDED`, successful `SUBMITTED` and observed native-fence
`COMPLETED` counters are distinct. Completion is not inferred from a loaded DLL
or an old log. The private launch harness creates a fresh log for every process.

## Developer checks

Build the existing offscreen shaders/checks first using
`Build-VulkanChecks.ps1`. On Windows, build the explicit layer with
`Build-LivePrototype.ps1 -Compiler <clang++.exe> -VulkanHeaders <headers-root>`.
Run `Test-LivePrototype.ps1` with those arguments plus `-Glslang <glslang.exe>`
and `-Validation <validation-layer-folder>`. Nothing is registered or installed.

The native-scene fixture generates its own graphics shader and images. It
compares 36 injected frames at 65x49, 257x145 and 3840x2160 against a separate
compute readback; 12 use two queue families and graphics-stage-scoped binary
signals. It also verifies the original unmodified cross-queue control, live
On/Off, UI-name bypass, single-layer array views/remaining ranges, two resizes,
native compute pipeline/descriptors/immutable samplers/dynamic offsets/push
values, GPU-fence completion and complete scratch retirement. Separate
allocation-refusal and missing-shader runs retain native pixels. These runs
passed standard and synchronization validation with zero warnings/errors on
the R9700. Submission wait/signal scopes are conservatively widened for added
compute/transfer work; the two-queue test also passed with the prior scopes,
so it has not identified a synchronization cause for the live exit.

All original 61 raster-frame checks, 45 diagonal reference cases and 35,035
camera-motion samples remain passing. Private 4K scene replay also passed:
compute/raster edge data was identical; 544 of 33,177,600 blend-weight channels
differed; final color differed by at most 3/255. The synthetic tolerance remains
tighter. The captured-scene tolerance permits a third byte value on at most
0.01% of channels to account for raster-interpolated versus compute-calculated
offset rounding. This comparison is not an AA ground truth or a flight FPS test.

## Opt-in live activation

The private test launcher requires current PASS build/test receipts and matching
DLL/source/shader hashes. A failed build, failed test or changed recorder source
prevents launching an older executable by mistake. It sets only process-local
Vulkan environment variables.
The layer additionally requires an exact `NATIVE_AA_EXE` match,
`NATIVE_AA_LIVE_TEST=1`, `DISABLE_XPFSR24=1`, a shader folder, a writable log path,
and a control file containing `ON`. Setting that file to `OFF` bypasses future
scene recordings without restarting. That file is a developer comparison
control. The new independent in-sim panel instead uses a versioned process-local
channel and saves validated choices; see [the controls guide](CONTROLS.md).

The owner's harness backs up simulator preferences, temporarily selects full
native input scale, and restores that one setting after a normal simulator exit
without overwriting other simulator edits. That private harness, captures and
saved preferences are excluded from publication. The development installer now
checks build/test/source hashes, installs the original XPLM panel and shader
variants, preserves other registry values, uses hash-named DLLs and can enable
selective ordinary-launch discovery. This is not a packaged end-user release or
a claim that every third-party launcher has been tested.

Before release, test real cockpit and external camera movement, text, thin
geometry, night lighting/clouds, texture detail, frame-time distributions,
minimize/resize, resource pressure and restart/removal. HDR10/scRGB, VR,
multiple windows, other X-Plane versions, Intel/NVIDIA and native Linux/Metal
remain separate validation gates. Camera-only motion research is not connected
to this spatial filter and is not a substitute for full object/cloud velocity.

## Live controls / FXAA update

The 2026-10-31 R9700 session exercised live SMAA/FXAA switching, On/Off,
quality selection, blend dragging, FXAA subpixel selection, save feedback and
restoring defaults. It confirmed 70,379 GPU-completed AA recordings and normal
process exit code 0, with an empty observed-exception log. The tracked scene
scratch allocation stayed at 134,095,888 bytes (about 128 MiB), including private
control constants. This is a stationary ground/cockpit session, not a full-flight
stress test, peak process VRAM measurement or per-method performance benchmark.

The native adapter fixture also checks ten live quality/method/strength cases
against a separate compute readback, plus UI-channel Off and zero-strength
identity. A separate 60-readback GPU fixture covers all four SMAA presets and
FXAA, feedback endpoints, flat red/blue colors and alpha; all synthetic diagonal
cases improved against the supersampled reference. Runtime validation passed
without standard or synchronization messages. The concurrent IPC fixture
performed 50,000 coherent settings/status updates; portable tests cover bounded
settings parsing and roundtrip persistence.

Activation tests now use the same Windows manifest generator as installation.
They cover an unrelated Vulkan executable, the exact selected simulator,
environment opt-out, disabled installation and an existing FSR startup claim.
The real ordinary-launch test caught a Unix-style relative DLL path rejected
by the Windows loader with error 87 before our adapter was entered. The path
was corrected to native separators and unrelated-application/activation checks
were rerun. A subsequent real X-Plane launch loaded the installed layer without
private injection environment variables, restored the saved Off/Ultra state,
and started processing when enabled in the panel. Wheel input over the panel
left the read-only simulator FOV at the same 100 degrees.

Twelve untouched native 4K screenshots were captured privately at the owner's
request: AA Off plus SMAA Low/Medium/High/Ultra and FXAA, in matching paused
cockpit/external views. MSAA 2x was retained and FSR/NR/FG bypassed. Runtime
selection acknowledgments and completion progression were recorded for each
capture. Files are deliberately excluded from the source/installation package.
They do not establish temporal stability or constitute TAA screenshots.
