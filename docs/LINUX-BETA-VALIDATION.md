# Native Linux AA beta validation

This work ports the existing native AA scene adapter and XPLM controls to Linux;
it is not a Windows binary, Wine wrapper or CPU-only diagnostic plugin.
It does not add the separate planned motion/shimmer/combined-AA feature set.

The native Linux build uses C++17, external X-Plane SDK 4.3.0 and Vulkan headers,
with explicit symbol exports for the Vulkan layer and five XPLM entry points.
The distribution provides Vulkan, OpenGL, the C++ runtime and libcrypto.so.3.
The SDK is a build dependency and is excluded from published archives.

An injected readback exposed compute push-constant loss on both tested Linux
software runtimes: the retained descriptor value was 300 but the native bias 17
was absent. The adapter now shadows and explicitly replays eligible compute-stage
push constants after restoring pipelines/descriptors and after depth capture.
The shadow is bounded to 256 bytes and validates every required word, layout
signature and lifetime. Mixed-stage updates, incompatible signature changes,
unknown extension data, missing bytes or excessive ranges bypass injection.
Both the legacy and core/KHR push-constant commands are intercepted. This avoids
depending on implicit preservation across inserted private pipelines. Its CPU
contracts and actual injected readbacks must pass before a package is released.

The per-user layer manifest points to a content-hashed library in the selected
simulator. Its layer/manifest name includes the simulator-root hash so separate
installations do not share a discovery name. Admission checks the actual
`/proc/self/exe`, the library's location and explicit installation marker.
FSR conflicts and opt-out bypass processing. Linux does not install signal handlers.

The controls and layer communicate through a 0600 UID/PID/process-start-time
mapping. Bounded atomic snapshots preserve coherent settings, status and camera
data. A locked reference count and unlinked-object retry handle DSO lifetime;
ordinary final close unlinks the mapping. Process-start identity avoids stale
prior-session status. A crash may leave a small old mapping until reboot, but
it cannot be reused by a different process-start identity.

## Release checks

The native Linux CI workflow must pass before release. It builds the layer,
plugin and shaders, runs portable contracts, concurrent channel snapshots,
offscreen algorithms and the actual injected-layer GPU fixture. The injected
fixture verifies current GPU pixels, alpha/state/descriptor restoration, TAA
history/reset, ownership and allocation/missing-shader bypass, with zero owned
scratch at teardown. Synchronization/standard validation messages are failures.
The hosted software Vulkan driver exposes one queue family. An explicit fixture
option exercises same-queue semaphore producer/consumer submissions there; it
does not establish distinct-family concurrency. The Windows R9700 fixture keeps
the strict two-family requirement and has passed those handoffs separately.

The extracted tarball is checked against its manifest and tested through initial
install, update retirement and uninstall. Tests cover path traversal and symlink
rejection before mutation, edited/unknown files and preference preservation,
plus actual Vulkan loader admission of the selected executable and isolation of
unrelated, opt-out, disabled-marker and conflicting-FSR processes.

The Linux package must contain only the original Linux `.so`/`.xpl`, 17 production
shaders, installer, plain instructions, notices and a sanitized test receipt.
No SDK runtime/library, Windows executable, proprietary model, simulator shader
or user log is release payload. The exact source commit and hashes are recorded.
Read the published CI result and package manifest for the particular beta; a
validation plan alone is not a passing result.

## Remaining validation

No Linux X-Plane flight was performed on the Windows test PC. Linux X-Plane
scene/shader identity can differ; the exact verified SDR gate is retained and
must bypass when it does not match. Passing hosted synthetic tests does not prove
that this boundary is reached in every Linux flight. Community current-session
logs are needed to verify this and support a different signature safely.

Debian version, kernel and driver data for the community's RX 9060 XT and separate
Radeon 8060S machine were unavailable. Those GPUs, all distro/driver combinations,
Steam/Flatpak sandbox discovery, HDR/PQ, VR/multi-view and long-flight behavior
are not established by this beta. Windows before/after captures remain labeled
as Windows. TAA still uses estimated camera/depth motion and may soften or trail.
