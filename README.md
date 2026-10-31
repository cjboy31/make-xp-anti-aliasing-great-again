<img src="assets/branding/aa-icon-128.png" alt="Make XP Anti-Aliasing Great Again icon" width="96" height="96">

# Make XP Anti-Aliasing Great Again

Independent, native-resolution anti-aliasing research for X-Plane 12.
The goal is a lightweight in-sim plugin for people who want smoother edges
without FSR, upscaling, neural rendering or frame generation.

**[Download 0.3.0 Windows x64 alpha](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v0.3.0-alpha).**
Extract the Windows ZIP asset, read `README-FIRST.txt`, close X-Plane and run
`NativeAA-Setup.exe`. Select your simulator folder, then use your usual
launcher. Open **Plugins > Native AA > Open AA controls**.
See [motion, shimmer and combination details](docs/MOTION-SHIMMER-COMBINATIONS.md)
and [setup](docs/CONTROLS.md). Previous versioned releases remain available.

**Linux native beta:** [0.3.0 Linux beta downloads](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v0.3.0-linux-beta.1)
and [Linux installation/testing scope](docs/LINUX-BETA-INSTALLATION.txt).
This includes a native `lin.xpl` panel and Vulkan `.so` layer with the existing
SMAA/FXAA/experimental TAA algorithms. Automated native build/IPC/injected-GPU
and package/loader checks are distinct from Linux X-Plane flight validation,
which remains open. A different simulator scene signature safely bypasses.

**New in 0.3:** TAA + SMAA / FXAA finishing, optional camera-seeded image-motion
refinement, shimmer stabilization and vector/history diagnostics. Use the
**Renderer** tab to save Native AA, FSR/NR/frame generation or the original
simulator for your next launch. Only one renderer processes a session; switching
requires restart and retains each mod's settings. The shared selector is merged
in both projects. Update Native AA to 0.3.0 and install
[FSR 0.7.3 Normal](https://github.com/cjboy31/xplane-fsr-vulkan/releases/tag/v0.7.3-alpha)
for load-order independent switching. See [coexistence](docs/RENDERER-SELECTION.md).

The offscreen Vulkan SMAA recorder has synthetic and private captured-scene
checks. A separate, explicitly enabled Windows scene adapter is undergoing
live testing against a captured X-Plane 12.4.4 SDR path. Unsupported paths
remain unchanged. See [live-test scope](docs/LIVE-TEST.md) and
[offscreen reproduction](docs/SMAA-PROTOTYPE.md).

The current release includes an original in-sim control panel, saved live SMAA
quality/blend controls, an independent FXAA 3.11 alternative and fresh
GPU-completion status. Open **Plugins > Native AA > Open AA controls** after
installing the checked build; see [controls and setup](docs/CONTROLS.md).

Experimental **native-resolution TAA** is connected to the Windows
scene adapter and live panel. It uses geometry jitter, a bounded depth snapshot,
camera/depth reprojection, color/depth rejection and clipped temporal history.
Camera motion is estimated; X-Plane does not supply general object/cloud vectors.
Sky/invalid depth uses fresh samples. Unsupported capture falls back visibly to
SMAA. Live Windows verification now confirms completed temporal frames, history
reuse, live controls and clean restart on X-Plane 12.4.4 / R9700. This is still an
experimental alpha build, not a general compatibility or long-flight guarantee.
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
| TAA + SMAA / FXAA | Temporal accumulation followed by adjustable spatial finishing | Extra GPU work/memory; stronger finishing may soften detail |

![SMAA High before and after](docs/showcase/smaa-high-comparison.png)

![FXAA before and after](docs/showcase/fxaa-comparison.png)

![Experimental TAA before and after](docs/showcase/taa-comparison.png)

See [capture conditions and limits](docs/showcase/README.md),
[live TAA validation](docs/TAA-LIVE-VALIDATION.md) and
[in-sim controls and build/install instructions](docs/CONTROLS.md).
**These screenshots showcase the Windows alpha. They are not Linux captures.
macOS rendering is not available yet.** These showcase images come from the
0.2 baseline; they are not a measured comparison of the 0.3 shimmer controls.

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
| Linux x64 | Native Vulkan scene processing | Native beta layer/panel/package; automated validation, Linux flights pending |
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
AA modes. Shared renderer selection avoids processing the same scene twice
and retains both projects' preferences; see [renderer selection](docs/RENDERER-SELECTION.md).

[Community Discord](https://discord.gg/yh396QNgHZ)

Original code is MIT licensed. Pinned SMAA (MIT) and FXAA (BSD/Apache notices)
sources retain their upstream licenses. No SDK runtime, model, simulator asset or user
configuration is bundled. See
[dependency research and notices](THIRD_PARTY.md).
