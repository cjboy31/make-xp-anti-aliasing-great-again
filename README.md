# X-Plane Native AA

Independent, native-resolution anti-aliasing research for X-Plane 12.
The goal is a lightweight in-sim plugin for people who want smoother edges
without FSR, upscaling, neural rendering or frame generation.

**Development foundation only. There is no working AA plugin, installer or
simulator rendering backend in this repository yet.** The portable C++ core
checks whether a frame is eligible for future processing; it does not filter
pixels, inject a renderer or change X-Plane settings. Build checks do not prove
simulator or GPU compatibility.

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
| Windows x64 | Vulkan scene processing | Portable core; renderer not implemented |
| Linux x64 | Native Vulkan scene processing | Portable core; renderer not implemented |
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

The tests reject reduced-resolution inputs, unsafe frame ownership, mismatched
sessions/devices, unresolved MSAA, unknown color/UI boundaries and insufficient
scratch budgets. They do not dispatch SMAA or test visual quality.

See [architecture](docs/ARCHITECTURE.md), [implementation plan](docs/ROADMAP.md),
[validation gates](docs/VALIDATION.md) and [contributing](CONTRIBUTING.md).

## Related project and community

The [FSR 3/4 + Frame Gen project](https://github.com/cjboy31/xplane-fsr-vulkan)
is maintained separately. This tool does not depend on its renderer or Native
AA modes. Future coexistence must avoid processing the same scene twice and
must not silently rewrite the other plugin's preferences.

[Community Discord](https://discord.gg/yh396QNgHZ)

Original code is MIT licensed. No third-party shader, SDK runtime, model,
simulator asset or user configuration is bundled. See
[dependency research and notices](THIRD_PARTY.md).
