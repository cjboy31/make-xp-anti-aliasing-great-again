# Motion vectors and AA alternatives

Research and first implementation milestone: 2026-10-31. This experiment is
separate from the tested spatial-AA milestone. The `experimental/live-taa`
branch now connects an original camera/depth TAA implementation to the Windows
scene adapter. Its injected-layer/offscreen checks and initial live Windows
integration checks pass; see [the live record](TAA-LIVE-VALIDATION.md).
Broader quality/stability evaluation remains open. It supplies estimated camera
motion with rejection, not general native object/cloud velocity.

## Source review and decisions

| Primary reference | Finding | Decision for this project |
| --- | --- | --- |
| [SMAA](https://github.com/iryoku/smaa) | MIT spatial/temporal variants; T2x needs projection jitter and temporal resolve | Keep the working spatial path; evaluate T2x only with verified jitter/motion |
| [Playdead temporal AA](https://github.com/playdeadgames/temporal) | MIT Unity implementation with camera velocity, moving-mesh support and history clipping | Useful algorithm reference; Unity scene/mesh hooks are not X-Plane hooks; no code imported |
| [X-Plane Motion Vectors V2](https://github.com/Vihaan2012-cmyk/Motion-Vectors-for-Xplane-12-V2) | GPL-3.0 project describes shader instrumentation and per-pixel motion, not just camera reprojection | Do not copy it into an MIT-only distribution; investigate a documented optional provider interface before any interoperation claim |
| [ReShade Motion Estimation](https://github.com/JakobPCoder/ReshadeMotionEstimation) / [license](https://github.com/JakobPCoder/ReshadeMotionEstimation/blob/main/LICENSE.md) | Image-based flow with CC BY-NC 4.0 terms | No code imported; estimated flow needs confidence and visibility checks, not blind replacement of scene velocity |
| [Intel CMAA2](https://github.com/GameTechDev/CMAA2) | Apache-2.0 spatial AA reference, DX11/DX12 HLSL; archived by Intel | Candidate for a later separately reviewed Vulkan/Metal port; not implemented |
| [NVIDIA FXAA whitepaper](https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf) | Single-pass spatial filtering on resolved scene color | Possible low-cost fallback; separately measure detail loss and review implementation licensing before import |

These are research references, not bundled plug-ins or endorsements. Public code
availability does not mean all files share our original MIT license. The only
imported graphics algorithms are the separately attributed SMAA and FXAA
sources listed in [the dependency notices](../THIRD_PARTY.md).

## Motion incorporated now

The original `camera_motion.comp` reconstructs a static point from current depth
and the inverse unjittered current view-projection matrix, then projects it into
the previous view. It uses one 8x8 compute dispatch and one output write per pixel.
The recorder neither allocates per frame nor waits, submits, reads back or scans
heaps. Constants and all image ownership/barriers remain with the caller.

The pass requires an explicit trusted-static mask. Unknown pixels, moving
objects, clouds, water, transparencies and reactive materials cannot be marked
static merely because depth exists. With no reliable mask/provider, use spatial
AA and invalidate temporal reuse. Camera matrices alone cannot supply moving
aircraft or cloud velocity. This limitation is inherent in the chosen inputs.

Inputs are native-size depth, a static mask and column-major, unjittered Vulkan
matrices with NDC depth [0,1] and +Y down. An adapter must explicitly convert other
conventions. Normal and reverse-Z clear depths are configurable. The output is
`(previous-minus-current pixel motion, projected previous depth, geometric valid)`.
Validity does not prove prior visibility: a future resolve must compare actual
previous depth and reject disocclusion before using color history.

Projection jitter is handled exactly once. Canonical motion excludes jitter;
history sampling adds `previousJitter - currentJitter` in native pixels. The
portable adapter normalizes pixel/UV/NDC units and vector sign/Y conventions.
It rejects previous-frame-grid optical flow because simply negating forward
flow is not a correct inverse field at current-frame pixels.

History eligibility requires the same nonzero session/history epoch, view/eye,
native size, consecutive source-frame sequence and verified native scene/jitter.
Camera cuts, origin/projection changes, missing frames, resize or unverified
inputs invalidate history. A session epoch must also change on flight/scene
replacement. Validity never comes from the saved checkbox or an old log.

The prototype uses RGBA32F output for readback precision. That is **not** a final
production memory choice: evaluate RG16F motion plus compact validity and reused
depth storage before full-resolution integration. No VRAM or FPS gain is claimed.

## Validation delivered and remaining

Local R9700 tests compared 35,035 GPU motion samples to an independent double-
precision CPU oracle. Cases include identity, translated camera, jitter changes,
reverse-Z clear depth, perspective/rotated cameras, static/moving masks, NaN
depth/matrices, reset after a valid frame and behind-camera/offscreen rejection.
All output values were finite,
validity matched, and valid vectors/depth matched within 0.0005 pixel/depth units.
Every invalid pixel was overwritten rather than retaining prior motion.

The same run retained all 61 SMAA frame checks and had zero standard/sync
validation warnings or errors. CPU tests also cover motion conversion, incorrect
flow-grid rejection and history reset gates. Linux Lavapipe and the four-platform
CPU CI run these checks; see Actions for each commit's actual result. These are
synthetic tests, not real X-Plane motion-field validation.

## Generic temporal-input experiment

The original `TemporalResolvePipeline` now consumes native-size current/history
color, canonical motion/predicted previous depth, actual previous depth and a
reactive mask. It adds jitter delta exactly once, samples history in linear
light, clips it in current-neighborhood luminance/chroma space, reduces feedback
with motion/reactivity, rejects invalid/outside/disoccluded pixels and preserves
current alpha. An immutable reset pipeline is selected from verified CPU frame
stamps; unverified jitter, cuts, changed view/session/size or sequence gaps cannot
reuse history. The recorder allocates no resources per frame and does not submit,
wait or read back; the caller owns images, constants and synchronization.

The new R9700 fixture checked 20 frames / 63,700 pixels against a separate
double-precision reference, including fractional jitter, translated vectors,
depth mismatch, reactive masks, NaN/Inf, out-of-bounds motion, history reset
reasons and alpha. It passed with zero standard/synchronization validation
messages. This establishes the resolve's behavior on supplied inputs, not live
motion quality or a working X-Plane TAA integration. Projection datarefs are
read-only and a cleared post-tonemap depth image is not reliable world depth.
No camera-control jitter, assumed-static whole-scene mask, blind history blend,
or imported GPL renderer is substituted to make a selectable checkbox.

1. Establish independent native scene color/depth and exact current/previous
   projection provenance while simulator FSR is off. Prove that render jitter is
   actually applied; changing a UI number or camera dataref is insufficient.
2. Prefer verified per-object scene vectors when available. Offer camera/depth
   vectors only for trusted static pixels. An external provider needs an explicit
   versioned format, current frame/session IDs, resource/sync ownership and tests
   for units, Y, sign, jitter, MSAA resolve and per-view identity.
3. Evaluate TAA and SMAA T2x at equal input/output dimensions. Use responsive
   masks, previous-depth disocclusion tests, bounded history, neighborhood/variance
   clipping, exposure handling and motion-aware feedback. Preserve alpha/UI.
4. On invalid history, retain current spatial AA. Do not accumulate uncertain
   clouds or fabricate object velocity from camera motion. Keep sharpness and
   temporal feedback separate; sharpening cannot repair an incorrect trajectory.
5. Test actual panning on the ground, moving aircraft, cloud edges, fine textures,
   cockpit text, scene changes, thin-line shimmer, HDR and memory pressure. Compare
   matched video and error/flow measurements, not a favorable still screenshot.

## Connected camera/depth TAA branch

The new `NativeTaaPipeline` uses a different, explicit history convention:
resolved color history is stored on an unjittered output grid. Native raw color
and depth retain their recorded geometry jitter. Previous jitter is used for
raw/depth lookups, never added a second time to resolved-color reprojection.
A centered eight-sample sequence is applied through native geometry viewports,
without writing projection datarefs or controlling the camera. UI and fullscreen
postprocessing viewports are restored. Only the captured native `gbuf-depth`
role, supported formats/layouts, full size, primary one-time command buffers
and matching fresh camera epochs are admitted.

Native D32S8 depth, including MSAA, is resolved into one R32F snapshot. TAA reuses
the spatial scratch images for raw/color history and adds one previous R32F depth
image. The existing 384 MiB budget is shared; no full multisampled frame cache or
motion-vector render target is allocated. The adapter restores depth/stencil
layouts and native compute state, widens inserted-work semaphore scopes, and
tracks temporal completions separately. Retired pipeline/layout state refuses
injection. Resize, view/session/epoch/projection changes, cuts, stale metadata,
sequence gaps, Off and spatial switching discard temporal reuse.

Camera matrices do not become object vectors. The shader rejects disoccluded
depth and raw-color changes and clips history in current-neighborhood luminance/
chroma bounds. Sky/invalid depth uses fresh samples. Clouds over geometry, water,
reflections and moving objects can still need a richer provider; residual
confidence is a heuristic, not proof of static geometry. This is why the mode
remains experimental and retains spatial fallback.

R9700 synthetic checks: 56 resolve frames, 16 actual sampled D32S8 depth readbacks
across supported 1x/2x/4x/8x counts, and zero standard/synchronization validation
messages. A separate supersampled diagonal reference gave fixed-grid error
0.00225904 with history versus 0.00273509 with fresh-only reconstruction.
These are synthetic errors, not simulator image-quality/FPS measurements.
The live-layer fixture adds 32 TAA frames with resets, both viewport Y directions,
resize, a same-frame unmodified alpha reference, native compute descriptor/
dynamic-offset/push-constant restoration and zero validation messages.

SMAA 1x remains the no-history baseline. Native MSAA may be a useful optional
pre-resolve partner; it addresses geometry coverage at a higher sample cost.
FXAA and CMAA2 are spatial alternatives to compare, not automatic filters to
stack. Texture mip/anisotropic filtering, shadow aliasing and low-resolution cloud
rendering need separate diagnosis rather than indiscriminate AA blur.
