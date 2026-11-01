# Implementation plan

## Foundation delivered

- Separate original MIT project with no FSR dependency.
- Portable frame-admission contract and CPU tests.
- CMake and four hosted OS/architecture build/test jobs.
- Platform-specific access and visual-validation gates.

## First offscreen rendering milestone

- Pinned MIT SMAA shader and lookup-table sources with verified hashes/notices.
- Three-pass Vulkan SMAA 1x recorder; native dimensions, sRGB-aware blending,
  preserved scene alpha and cleared intermediate images.
- Original synthetic reference/readback checks, reuse/teardown and current-run
  result status. Linux software-Vulkan validation CI is configured.
- Local AMD R9700 run: 61 frames, all 45 diagonal-reference cases improved,
  zero standard/synchronization validation errors or warnings. These are not
  X-Plane screenshots or flight-quality/performance evidence.

## First working prototype

The `experimental/live-smaa` branch now has an opt-in Windows scene adapter
undergoing private live checks. It uses the captured native 12.4.4 SDR boundary,
independent compute SMAA, bounded scratch and explicit state/submission handling.
The first live exit and reproduced descriptor-parser correction are documented
in [live-test evidence](LIVE-TEST.md); this remains a developer experiment.

1. Extend the offscreen Vulkan/reference checks to more scene patterns and drivers.
2. Find and verify an independent native-resolution X-Plane Vulkan scene boundary
   with FSR disabled, before UI. Test both Windows and native Linux activation.
   Bypass when ambiguous instead of guessing another image.
3. Add bounded scratch resources, barriers, completion ownership, HDR-aware
   handling, resize/minimize/device-loss recovery and clean unload.
4. In-sim menu, live On/Off, four SMAA presets, FXAA, blend/subpixel controls,
   selected-state/help feedback, atomic saved choices and current-session
   completion diagnostics are implemented on the Windows development branch.
5. Measure matched flights, text preservation, shimmer and memory/frame times
   against native Off/MSAA settings. Keep performance claims tied to evidence.

## macOS workstream

Prove native Metal scene-color/composition access first. Then implement Metal
shader/data and synchronization equivalents, validate Apple Silicon and Intel
hardware separately, and address signing/notarization and installation. Shared
CPU code passing on both architectures does not complete this workstream.

## Later evaluations

- MSAA + spatial AA presets, with measured quality/cost and no forced resolution.
- Temporal AA only with reliable motion/jitter, disocclusion, responsive masking,
  camera-cut/history invalidation and stable cloud/object motion.
- Per-view support for VR/multiple monitors after each presentation path is proven.

The owner's motion-vector/TAA follow-up is now isolated on
`experimental/temporal-aa`, based on the SMAA prototype. Original camera/depth
GPU vectors, convention normalization and history gates are implemented and
tested offscreen. An original TAA resolve has also passed GPU/reference checks;
the `experimental/live-taa` branch now connects native geometry jitter and
camera/depth reprojection to the in-sim panel. Live Windows completion, controls,
panning, method switching and restart are verified in
[the TAA validation record](TAA-LIVE-VALIDATION.md). Per-object/cloud motion,
SMAA T2x sampling and live provider interoperation remain pending. See
[the research and integration plan](MOTION-AND-TEMPORAL-AA.md).

No dates, GPU support guarantees or installable rendering release are promised
by this plan. Track implementation evidence in this repository's issues.
