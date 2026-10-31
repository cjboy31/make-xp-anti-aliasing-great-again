# Proposed architecture

## Independent pipeline

Native simulator scene -> verified resolve/tone-map boundary -> spatial AA ->
simulator UI/composition -> presentation. Input and output dimensions are equal.
If the native boundary cannot be proved, retain the original frame and display
the reason. Do not repurpose an FSR/upscaling dispatch to make this work.

First candidate: SMAA 1x edge detection, blend-weight calculation and neighborhood
blending. Start with a known SDR color domain. An HDR path needs deliberate
edge-detection/perceptual conversion, alpha and output-transfer handling; passing
PQ or scene-linear values through an SDR shader is not acceptable.

The portable `native_aa_core` contains CPU frame-admission contracts only.
`MayRecord` is not a successful dispatch or an Active status. All backends are
currently absent. A future Vulkan or Metal adapter must validate actual image
formats/usages, device ownership, barriers and completion, and return evidence
from the current frame/session before UI can say AA is active.

## Graphics adapters

Windows and Linux need native Vulkan interception with independent activation
and a verified full-resolution color path while simulator FSR is disabled. Prove
the interception point with captures and fenced readbacks; avoid image-size-only
heuristics, private SDK callback abuse or blindly filtering the final UI image.
Linux needs ELF/shared-library activation and distribution-specific loader tests.

macOS needs a native Metal access/composition design. The public plugin UI APIs
do not, by themselves, supply the scene image. Obtain a supported contract or
document a separate viable approach before promising this backend. MoltenVK,
compiling this C++ core and loading a diagnostic plugin are insufficient proof.

The baseline must retain a real source frame on unsupported input, GPU pressure
or failed dispatch. Use bounded per-frame scratch resources, completion-gated
reuse and explicit resize/device-loss teardown. No per-frame device-wide wait,
readback or heap scan is allowed in the eventual production fast path. The
current CPU core does none of these operations and allocates no graphics memory.

## UI and settings (planned)

Primary UI lives in X-Plane. Use native UI hooks where suitable with a shared
portable settings model, concise help, toggles and quality controls. Store AA
choices separately from the FSR project. Observe actual simulator state when
checking native resolution; equal final input/output dimensions alone do not
prove the scene was rendered natively.

MSAA is an independent simulator option; resolve its samples before SMAA.
Offer measured combinations only after validation. Never silently rewrite
joystick, camera, UI scale, another add-on's settings or renderer environment.
Coexistence checks must visibly bypass conflicting active reconstruction. Saving
a choice is distinct from applying it, and both are distinct from rendering.
