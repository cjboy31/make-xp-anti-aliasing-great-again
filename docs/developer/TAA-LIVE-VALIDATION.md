# Experimental native TAA: live validation

Test date: 2026-10-31. Windows x64, X-Plane 12.4.4-r1, AMD Radeon AI PRO R9700,
3840x2160 Full input, native simulator MSAA 2x, FXAA off, FSR/NR/FG off.
This is a bounded developer integration, not broad aircraft/GPU/VR compatibility.

## Corrections verified

The actual native depth is mutable-format D32S8 with 2x MSAA. Its depth-only
sampling view keeps the same format; only the mutable-format creation flag is
admitted. The native mono geometry also submits two identical viewports. Both
the live trace and the prior private RenderDoc command stream confirmed this.
The adapter now jitters and restores that exact pair. Distinct viewport arrays,
unsupported secondary/state paths and unknown scene roles remain excluded.

The injected regression uses mutable depth and single/identical-paired viewports
in both Y directions. It completed 32 temporal frames with actual history reuse,
reset/resize, exact current alpha and native compute-state restoration. The
46 existing spatial injections, failure bypass, zero scratch retirement,
50,000 coherent IPC snapshots and settings checks also passed with zero Vulkan
standard/synchronization validation messages. Hosted portable CPU and Linux
offscreen Vulkan jobs passed for commit fb6322b2f8eabac3733183076deaf8e2fce5d8eb.
These hosted jobs do not establish Linux simulator or macOS Metal integration.

## Actual simulator result

One corrected flight completed over 25,000 temporal frames and over 33,000
total AA frames. Temporal reuse advanced independently of spatial completions.
The panel displayed TAA ACTIVE only after fresh temporal GPU completions.
On/Off, TAA -> SMAA -> FXAA -> TAA, live history weight 85 -> 50 -> 85,
history-reset epoch, external horizontal/downward ground panning and a cockpit
view change were exercised. No exception entry was recorded in this session.
The session exited normally with code 0. A second ordinary installed launch
retained TAA On, blend 100% and history 85%, and confirmed over 470 fresh
temporal completions with history reuse and the same scratch reservation.
This is a short ground test, not a
long-flight stability test, objective shimmer reduction or frame-time benchmark.

Tracked AA scratch stayed at 200,942,832 bytes (about 192 MiB at native 4K).
The 384 MiB cap is shared across the layer. This excludes native simulator
textures, driver memory and pipeline allocations; it is not total VRAM use.

Unmodified native PNG captures avoid Windows desktop HDR screenshot washout.
The external Off/TAA/SMAA High/FXAA set has exactly matching recorded view,
projection, FOV, dimensions and pause-state data. The cockpit pair had camera
movement and is excluded from matched comparison claims. Stills do not prove
moving-image shimmer, blur, ghosting or performance improvements.

## Known limits

Camera/depth reprojection estimates motion; it does not supply native animated
object or cloud velocity. Raw-color rejection is heuristic. Moving parts,
reflections, translucency and clouds over geometry can still need richer motion
or responsive masks. Clear/invalid depth uses fresh color. TAA may soften detail.
Start with history 85%; lower it for faster response, and use live On/Off to
judge the same view. SMAA is still the default for a new installation.

The earlier saved-737-situation launch exited before the scene/AA path existed
with a heap-corruption code. That is recorded separately and is not attributed
to a particular plugin without proof. Normal startup with the stock SR22 was
used for these TAA checks.
