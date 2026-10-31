# Native AA comparison showcase

Make XP Anti-Aliasing Great Again: initial Windows development build.

Each PNG contains Before (top) and After (bottom), cropped at the same native
pixel coordinates: x=1280, y=900, width=1280, height=320 from 3840x2160 originals.
The screenshot regions are unscaled and losslessly stored; only label bands
were added outside them. No AI editing, sharpening or exposure changes.

The recorded view, camera/projection matrices, FOV, resolution and pause state
match exactly across all four external captures. Aircraft: stock Cirrus SR22,
ground view, X-Plane 12.4.4-r1, Windows x64 / AMD Radeon AI PRO R9700. Simulator
MSAA 2x stays enabled; native FXAA, FSR, NR and frame generation are off.
AA blend 100%, SMAA High, FXAA subpixel 75%, TAA history 85%. Live renderer
acknowledgements were checked, with fresh temporal GPU completions for TAA.

SMAA and FXAA address spatial edges. TAA also reuses valid geometry history.
These stills do not measure shimmer in motion, ghosting or performance. They
also show the softness trade-off rather than hiding it. Broader motion-vector
and AA-combination work follows separately. A moving cockpit capture pair was
excluded because it failed the camera-match check.

The owner's selected simulator screenshots are published as project
illustrations. The project's MIT source license does not grant rights to
Laminar Research or other depicted third-party artwork/assets. No raw simulator
assets, shader bytes, personal logs or configuration files are included here.

## Original-image SHA-256

| Capture | SHA-256 |
| --- | --- |
| Off | e071d4c2e89f9a191266a51c20b450c33420961dbdcdd42cf00f504ccf65a69a |
| SMAA High | fe1bcc5996fced6c8a917213ac05180bf6341e73aeb6f0fce5622c9c7bc39886 |
| FXAA | b309f9440cbc97b041e32de7d4e8d7e16d3677c52c70b7896005622bfb3e7074 |
| TAA | 7d59081e26d8c2e79a294034ebbcbeaa4f829ef94c4bd4b6e26fc62d53bb6d8c |
