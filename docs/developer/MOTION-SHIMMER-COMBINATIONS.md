# Motion assistance, shimmer stabilization and AA combinations

Development update, 2026-10-31. Native resolution throughout; no FSR, neural
model or frame generation. Windows and native Linux use the same shader and
settings implementation. The published 0.2.0 builds remain the earlier baseline.

## Controls

Open Plugins > Native AA > Open AA controls. The AA modes & combinations tab
offers SMAA, FXAA, TAA, TAA + SMAA and TAA + FXAA. All selections apply live and
save to Output/preferences/NativeAA.ini. X-Plane MSAA can remain enabled with
any method; configure it in X-Plane Graphics. Use Full input and native FXAA Off.

TAA + SMAA is a temporal resolve followed by an adjustable SMAA finishing pass.
TAA + FXAA uses FXAA for that final pass. These are this project's combinations,
not SMAA T2x/4x. The finishing slider starts at 50%; more filtering can soften
detail. Quality buttons affect SMAA, including its combined mode. Set finishing
to 0% for the same temporal output without spatial finishing. Unfiltered TAA
output is stored as history before spatial finishing, preventing recursive
spatial blur. Original raw-color and depth history remain separate.

The Motion & shimmer tab provides:

- Camera + depth: estimated previous-minus-current motion on the unjittered
  native grid, using world/body transforms and captured reverse-Z depth.
- Camera + image refinement: optional bounded local gradient-based alignment
  around that estimate. It costs extra GPU sampling and is off by default.
  A well-conditioned 2D gradient, improved patch error, small reverse residual,
  maximum two-pixel displacement, valid bounds and depth agreement are required.
  Failed/ambiguous refinement retains the camera estimate and normal rejection.
- Shimmer stabilization, 0..100%: gives slowly moving fine patterns more history
  confidence when their local luminance/chroma mean agrees and depth is valid.
  0% retains the original rejection; start at 50%. It cannot reconstruct detail
  never sampled by the renderer, remove every shimmer, or fix texture/shadow/
  cloud sampling at source. Turn it down if a moving pattern retains history.
- History weight, 0..95%: controls temporal accumulation independently of the
  stabilization heuristic. Lower values respond faster; higher can soften detail.
- Scene image / Estimated vectors / History confidence: live scene diagnostics.
  Vector red/green channels encode signed X/Y; neutral is zero, black is invalid.
  Confidence green means more accepted history; red means fresh samples. Return
  to Scene image to view AA. Debug RGB is never stored in color history and
  mode changes reset temporal reuse.

These vectors are estimates, not native per-object/cloud velocities. Sky/invalid
depth uses fresh color; clouds over geometry, moving parts, transparencies and
reflections remain difficult. Large motion, ambiguous texture, exposure changes
and disocclusion reject history. There is no external vector-provider support
claim or imported GPL/noncommercial motion-estimation code.

## Implementation and ownership

Motion/refinement and stabilization run inside the existing 8x8 temporal resolve.
They add no full-resolution motion target or per-frame allocation. Combined
modes lazily cache three native RGBA8 finishing targets: approximately 95 MiB
extra at 4K, under the existing layer-wide 384 MiB image/staging budget. This
budget excludes simulator/driver memory. Cached resources retire with their
Surface owners, not when an earlier recording may still use them. Failed new
pipeline variants do not destroy the existing GPU cache. Allocation refusal
retains the guarded spatial fallback.

History resets on changed settings, size, session/epoch, source-frame gap,
camera/projection change, viewport Y convention and mode changes. Both IPC
implementations use a new v3 namespace so an older panel cannot interpret the
expanded control layout. Old preference files retain their supported selections;
new fields receive bounded defaults. Current-frame alpha and native compute
pipeline/descriptors/dynamic offsets/push constants are preserved.

## Evidence so far

Local R9700 GPU checks passed with zero standard/synchronization validation
messages: 62 temporal resolve frames and 16 D32S8/MSAA depth cases. The known
(+.7,-.4)-pixel synthetic image shift matched within .3 pixel at 3037/3969
interior pixels; mean error across all interior pixels was .284 pixel, including
refinement refusals. A three-phase fine-coverage test reduced mean-reference
error to .0497 times the baseline; genuine color change and disocclusion tests
retained current pixels. These are synthetic pattern results, not measured
simulator shimmer or general motion accuracy.

Injected-layer checks include the existing 32 temporal and 46 spatial frames,
15 exact independently recorded TAA-then-spatial references across all SMAA
qualities/FXAA and 0/50/100% finishing, 24 motion/stabilization/debug/switch cases,
12 distinct-family semaphore handoffs, coherent 50k-update IPC, settings migration,
native state restoration and failure-bypass/zero-scratch teardown.

The Windows X-Plane 12.4.4-r1/R9700 flight test completed over 35,000 temporal
frames and 42,000 total AA frames. At native 3840x2160 input with simulator MSAA
2x and FXAA Off, checks covered TAA, TAA + SMAA, TAA + FXAA, live On/Off,
refinement, vector/confidence diagnostics, shimmer controls, external ground
panning and cockpit return. The exception observer recorded no faults and
X-Plane recorded a normal shutdown. Matched native captures have identical
recorded camera/projection/FOV/pause state; these stills do not measure temporal
shimmer or performance. Layer scratch was about 287 MiB in combined modes.

The motion/combination source at `a64d8f1` passed hosted portable CPU, Vulkan and
native Linux builds on Ubuntu 22.04, with identical baseline binaries retested on
Ubuntu 24.04. Linux package hashes, native x86_64 ELF files, shader inventory and
installation lifecycle were independently checked. Physical Linux X-Plane
flights remain unverified. Renderer-selection changes require their own updated
checks; see [renderer selection](RENDERER-SELECTION.md).

## Primary research references

- [Lucas and Kanade, IJCAI 1981](https://publications.ri.cmu.edu/an-iterative-image-registration-technique-with-an-application-to-stereo-vision-ijcai):
  intensity-gradient local image registration. Our bounded GLSL implementation
  is original; no reference mod or tracking-library code is bundled.
- [Playdead temporal AA](https://github.com/playdeadgames/temporal): camera/object
  motion and history-clipping reference; Unity integration is not an X-Plane API.
- [SMAA authors](https://www.iryoku.com/smaa/): spatial/temporal sampling variants.
  Our optional finishing pass remains the attributed SMAA 1x implementation.
- [Laminar plugin drawing guidance](https://developer.x-plane.com/article/plugin-guidance-for-opengl-drawing/):
  matrix provenance, read-only view state and reversed depth conventions. We do
  not write projection/camera datarefs or install controller bindings.
