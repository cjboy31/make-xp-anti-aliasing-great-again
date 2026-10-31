# Native AA - in-sim controls (Windows development build)

Open **Plugins > Native AA > Open AA controls**. Enable or disable processing
with the ON/OFF button. Changes apply live and save automatically to
`Output/preferences/NativeAA.ini` in this simulator installation.

Developer build order (Windows x64): `Build-VulkanChecks.ps1`,
`Build-LivePrototype.ps1`, `Test-LivePrototype.ps1`, `Test-Activation.ps1`,
`Build-Plugin.ps1`, then `Install-DevelopmentBuild.ps1 -Simulator <X-Plane root>`.
The scripts take explicit compiler, Vulkan-header, glslang, validation-layer and
external X-Plane SDK paths. Installation refuses stale build/test/hash receipts.
Only this plugin's own per-user Vulkan value is written; existing registry keys
are preserved. Renderer DLL names include their content hash so an update does
not overwrite a DLL held by another application. Previous owned files are
removed only within the checked plugin directory and when their hash matches
the previous inventory; modified/unknown user files are retained.

- SMAA 1x: start with High. Low/Medium use shorter searches; Ultra searches
  farther and costs more. These are spatial presets, not extra temporal samples.
- FXAA 3.11: an alternative spatial method. Blend controls its overall effect;
  lower subpixel filtering preserves more fine detail, higher filtering softens
  small-scale aliasing. A perceptual-luma prepass detects red/blue edges too.
- AA Blend: 100% applies the selected result; 0% retains the native image.
- TAA (experimental): native-resolution temporal reconstruction using jittered
  geometry and camera/depth motion estimates. Select it live; no FSR or restart
  is required. History weight 0..95% balances response against accumulation.
  Reset temporal history retries capture and discards the old history epoch.
  Moving pixels are rejected using depth and raw-color residuals. General
  object/cloud motion is not available; this is not a promise of no ghosting.
  Unsupported input uses a clearly identified SMAA fallback.
- Restore defaults: SMAA High, enabled, 100% blend, 75% FXAA subpixel filtering.

Set X-Plane's input/render scale to **Full** (native resolution). Do not enable
another FSR/DLSS/XeSS/NR/FG renderer in this independent AA session. Native AA
does not upscale. Keep the output resolution and UI size you normally use.
You may retain X-Plane MSAA; configure it in X-Plane Graphics. Turn native FXAA
off when comparing our spatial methods so filters are not stacked accidentally.

AA ACTIVE requires newly GPU-completed AA frames in this process, a current
renderer heartbeat and the matching control sequence. OFF, WAITING, BYPASSED
and RENDERER UNAVAILABLE have separate meanings. A saved selection alone does
not prove that AA is running. Counters reset with each simulator process;
renderer logs use new process/session filenames under `Output/NativeAA`.
TAA ACTIVE requires its own new fenced temporal completions. A saved TAA choice,
SMAA fallback or completed spatial frame cannot establish temporal activity.
Camera metadata is read through public datarefs; no camera control or projection
dataref is written. Jitter is scoped to admitted native geometry viewports.

The installed per-user Vulkan discovery entry admits only the exact simulator
beside this plugin and its explicit standalone marker. It can request a
process-only bypass of the separate FSR project before renderer startup;
it does not edit that project's preferences or registration. If another
renderer has already claimed startup, processing is refused rather than stacked.
The FSR project's explicit launcher remains a separate choice; Native AA
refuses a session already selected by that launcher. Do not run both renderers
on the same scene. To disable ordinary-launch discovery, close X-Plane and
change `Resources/plugins/NativeAA/standalone.enabled` to `NativeAAStandalone=0`.
This leaves the independent FSR project available and retains AA preferences.
The current scene adapter is validated for the captured X-Plane 12.4.4 SDR
path. Other scene signatures, output sizes, HDR/PQ paths or extensions can bypass.

Optional commands `native_aa/toggle` and `native_aa/open_panel` can be bound
by the user in X-Plane. No bindings are installed automatically. The plugin
does not control the camera, joystick, aircraft, resolution or UI scaling.

This is a Windows developer build, not a claim of Linux/Metal simulator support.
Source, licenses and build instructions: https://github.com/cjboy31/xplane-native-aa
