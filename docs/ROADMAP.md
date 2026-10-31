# Implementation plan

## Foundation delivered

- Separate original MIT project with no FSR dependency.
- Portable frame-admission contract and CPU tests.
- CMake and four hosted OS/architecture build/test jobs.
- Platform-specific access and visual-validation gates.

## First working prototype

1. Pin and license-review SMAA shader/lookup-table sources; compile an isolated
   Vulkan test with known test patterns and CPU/GPU reference readbacks.
2. Find and verify an independent native-resolution X-Plane Vulkan scene boundary
   with FSR disabled, before UI. Test both Windows and native Linux activation.
   Bypass when ambiguous instead of guessing another image.
3. Add bounded scratch resources, barriers, completion ownership, HDR-aware
   handling, resize/minimize/device-loss recovery and clean unload.
4. Add the in-sim menu, On/Off comparison, quality controls, selected-state/help
   feedback, saved choices and fresh current-session activity diagnostics.
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

No dates, GPU support guarantees or installable rendering release are promised
by this plan. Track implementation evidence in this repository's issues.
