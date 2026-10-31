# Motion sources and shimmer work

Research checked 31 October 2026. This is a developer note, not a download claim.

## Installable community source

[Motion Vectors for X-Plane 12 V2](https://github.com/Vihaan2012-cmyk/Motion-Vectors-for-Xplane-12-V2)
is Windows/Vulkan software with its own launcher, shader changes, motion field,
TAA resolve and memory-management policies. Its README reports remaining temporal
instability. It is GPL-3.0. No code or binaries were copied into this MIT project.
It does not document a standalone cross-mod vector export that this plugin can
consume. Installing it alongside our scene adapter is not a supported way to add
vectors: both would try to alter the same scene and its history.

The correct integration choices are a separately selected renderer, an agreed
bounded vector-sharing interface, or an original motion implementation. Do not
advertise a plugin installation as true per-object vector support in Native AA.
The existing shared selector only coordinates Native AA and the companion FSR
mod, not arbitrary third-party injectors.

## AMD optical flow

[AMD's optical-flow documentation](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/optical-flow/)
describes motion estimates from consecutive images. This is developer code to
integrate, not an installable X-Plane motion-vector driver. It is distinct from
geometry/object velocity and does not supply missing visibility information.
The local SDK was located and inspected as a reference. No FSR runtime is added
to independent Native AA by this research.

## Implemented today

Native AA already has camera/depth reprojection, optional original local image
matching, depth/color rejection, history clipping and bounded shimmer controls.
These are estimates, not general native object, vegetation or cloud velocities.
TAA + SMAA / FXAA keeps unfiltered TAA history before spatial finishing.

## Next rendering priority

Measure moving fence/runway lines, vegetation and shadows in identical camera
paths. Compare TAA history 70/85/95, shimmer 0/50/100, both motion choices and
TAA versus combined modes. Record temporal flicker alongside retained fine
detail, ghosting, GPU time and scratch memory. Improve confidence and rejection
only where these measurements show a benefit. No universal shimmer fix is
claimed by a UI change or a still comparison.
