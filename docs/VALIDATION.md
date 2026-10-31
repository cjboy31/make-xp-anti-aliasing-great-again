# Release gates

| Evidence | Windows Vulkan | Linux Vulkan | macOS Metal |
| --- | --- | --- | --- |
| Portable core CI | Configured; see Actions for result | Configured; see Actions for result | Configured for arm64 and Intel; see Actions for result |
| Offscreen AA shader/reference image | R9700 synthetic validation passed | Lavapipe validation CI configured; see Actions | Not implemented |
| Independent native X-Plane scene/UI access | Not validated | Not validated | Not established |
| Real flight quality/timing/memory | Not tested | Not tested | Not tested |
| Install/update/removal package | Not available | Not available | Not available |

For every claimed backend, record simulator version, driver, actual input/output
scale, graphics settings and test-scene conditions. Compare stationary and moving
matched views with exposure locked; do not compare raw HDR captures to SDR images.
Test cockpit screens/text, fences/power lines, trees, clouds/horizon, night lights,
ground-level panning, aircraft motion, camera cuts, menus, plugin windows and
third-party launchers. Separate geometry/texture/shadow aliasing from low-resolution
cloud rendering. Record regressions as well as improvements.

Check On/Off, Native MSAA Off/2x/4x/8x where supported, resize/fullscreen/windowed,
minimize, multiple monitors, session restart, interrupted updates, clean removal,
resource pressure and unsupported input. A busy or invalid frame must preserve
the source, not present uninitialized output. A stale prior-session log must never
make the current UI say Active. No controller/camera behavior may be changed.

Record CPU/GPU source-frame time, presented-frame time, memory and frame-time
distribution separately. Do not infer a gain from a screenshot or filter-only
benchmark. Publish only original/licensed source and redistributable dependencies;
exclude simulator assets, user settings/logs, proprietary DLLs and model weights.
