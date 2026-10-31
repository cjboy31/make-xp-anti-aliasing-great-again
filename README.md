# X-Plane Native AA

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

The temporal-AA experiment now includes an original, offscreen GPU temporal
resolve with reprojection, depth/reactive rejection, neighborhood clipping and
history-reset gates. It does **not** yet consume verified live simulator
motion/jitter inputs, so TAA is unavailable in the live panel. See
[motion research and AA alternatives](docs/MOTION-AND-TEMPORAL-AA.md).

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

A spatial filter cannot promise to remove all temporal shimmer, cloud
undersampling, texture aliasing or shadow artifacts. Temporal AA is a separate
future evaluation requiring verified jitter/motion and history invalidation.

## Platform targets

| Target | Intended integration | Current state |
| --- | --- | --- |
| Windows x64 | Vulkan scene processing | R9700 offscreen tests; exact-version adapter under live evaluation |
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
