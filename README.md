<img src="assets/branding/aa-icon-128.png" alt="Make XP Anti-Aliasing Great Again icon" width="96" height="96">

# Make XP Anti-Aliasing Great Again

Independent, native-resolution anti-aliasing research for X-Plane 12.
The goal is a lightweight in-sim plugin for people who want smoother edges
without FSR, upscaling, neural rendering or frame generation.

**Windows development build; no packaged end-user rendering release yet.**
The offscreen Vulkan SMAA recorder has synthetic and private captured-scene
checks. A separate, explicitly enabled Windows scene adapter is undergoing
live testing against a captured X-Plane 12.4.4 SDR path. Unsupported paths
remain unchanged. See [live-test scope](docs/LIVE-TEST.md) and
[offscreen reproduction](docs/SMAA-PROTOTYPE.md).

The current branch adds an original in-sim control panel, saved live SMAA
quality/blend controls, an independent FXAA 3.11 alternative and fresh
GPU-completion status. Open **Plugins > Native AA > Open AA controls** after
installing the checked developer build; see [controls and setup](docs/CONTROLS.md).

This branch connects experimental **native-resolution TAA** to the Windows
scene adapter and live panel. It uses geometry jitter, a bounded depth snapshot,
camera/depth reprojection, color/depth rejection and clipped temporal history.
Camera motion is estimated; X-Plane does not supply general object/cloud vectors.
Sky/invalid depth uses fresh samples. Unsupported capture falls back visibly to
SMAA. Live Windows verification now confirms completed temporal frames, history
reuse, live controls and clean restart on X-Plane 12.4.4 / R9700. This is still an
experimental developer build, not a general compatibility or long-flight guarantee.
See [motion research and AA alternatives](docs/MOTION-AND-TEMPORAL-AA.md).

## See the difference

Matched native 4K X-Plane screenshots: identical recorded camera/projection,
Full input and simulator MSAA 2x retained. These are 1:1 pixel crops with labels;
no sharpening, exposure adjustment or image enhancement was applied. Click each
image to inspect it. Stills show edge/detail differences, not measured reductions
in moving shimmer, ghosting or frame time.

| Method | Intended use | Trade-off |
| --- | --- | --- |
| SMAA High | Smooth aircraft outlines and geometric stair-steps without history | Cannot stabilize all temporal shimmer |
| FXAA 3.11 | A simple alternative for broad spatial edge smoothing | Can soften fine texture/text detail |
| TAA experimental | Accumulate native subpixel geometry samples over time | Can soften detail; estimated motion has limits |

![SMAA High before and after](docs/showcase/smaa-high-comparison.png)

![FXAA before and after](docs/showcase/fxaa-comparison.png)

![Experimental TAA before and after](docs/showcase/taa-comparison.png)

See [capture conditions and limits](docs/showcase/README.md),
[live TAA validation](docs/TAA-LIVE-VALIDATION.md) and
[in-sim controls and build/install instructions](docs/CONTROLS.md).
**These screenshots showcase the Windows development build. There is no
packaged general-release installer or working Linux/macOS scene adapter yet.**

## Direction

- Start with native-resolution **SMAA 1x** spatial AA. It does not need motion
  vectors or temporal history. Evaluate optional native simulator MSAA alongside
  it, using the simulator's supported settings.
- Keep aircraft displays, menus and plugin text outside scene AA where a verified
  scene/UI boundary is available. Bypass unsupported paths visibly.
- Provide an in-sim On/Off comparison, quality controls and current-session
  diagnostics. Preserve saved choices and support normal/third-party launchers.
- Measure texture detail, moving thin lines, clouds, HDR, memory and frame time
  before making quality or performance claims.

A spatial filter cannot promise to remove temporal shimmer, cloud undersampling,
texture aliasing or shadow artifacts. Experimental TAA addresses temporal sampling
with verified geometry jitter and history rejection; general object/cloud motion
remains a separate workstream. Sharpening is not a substitute for correct history.

## Platform targets

| Target | Intended integration | Current state |
| --- | --- | --- |
| Windows x64 | Vulkan scene processing | Initial live SMAA/FXAA/TAA checks on 12.4.4 / R9700; broader compatibility open |
| Linux x64 | Native Vulkan scene processing | Offscreen Vulkan fixture/CI; no simulator integration |
| macOS arm64 / x86_64 | Native Metal scene processing | Portable core; Metal scene-access investigation required |

The CI matrix builds and runs CPU contract tests on Windows, Linux, Apple
Silicon and Intel Mac runners. Passing these checks is **not** a working
X-Plane rendering release. A diagnostic Mac plugin or a Vulkan compatibility
layer would not establish Metal rendering support.

## Build the developer checks

Install CMake 3.20+ and a C++17 compiler. No graphics SDK, simulator, network
download or vendor runtime is needed for this core:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The CPU tests reject reduced-resolution inputs, unsafe frame ownership, mismatched
sessions/devices, unresolved MSAA, unknown color/UI boundaries and insufficient
scratch budgets. Enable `NATIVE_AA_BUILD_VULKAN_TESTS` for the separate offscreen
GPU checks described in [the prototype guide](docs/SMAA-PROTOTYPE.md).

See [architecture](docs/ARCHITECTURE.md), [implementation plan](docs/ROADMAP.md),
[validation gates](docs/VALIDATION.md) and [contributing](CONTRIBUTING.md).

## Related project and community

The [FSR 3/4 + Frame Gen project](https://github.com/cjboy31/xplane-fsr-vulkan)
is maintained separately. This tool does not depend on its renderer or Native
AA modes. Future coexistence must avoid processing the same scene twice and
must not silently rewrite the other plugin's preferences.

[Community Discord](https://discord.gg/yh396QNgHZ)

Original code is MIT licensed. Pinned SMAA (MIT) and FXAA (BSD/Apache notices)
sources retain their upstream licenses. No SDK runtime, model, simulator asset or user
configuration is bundled. See
[dependency research and notices](THIRD_PARTY.md).
