MAKE XP ANTI-ALIASING GREAT AGAIN - 0.2.0-alpha
Windows x64 / X-Plane 12 / Vulkan

WHAT IT DOES
Native-resolution scene anti-aliasing with live in-sim controls:
- SMAA 1x: Low, Medium, High and Ultra. Start with High.
- FXAA 3.11: adjustable blend and subpixel filtering.
- Experimental TAA: jittered geometry, camera/depth reprojection, clipped
  history, adjustable history weight and live history reset.
- Live On/Off comparisons, automatically saved choices and current-session
  GPU completion status.
This is independent of FSR. It does not upscale or generate frames.

INSTALL (NO COMPILER, SDK OR ADMINISTRATOR REQUIRED)
1. Close X-Plane normally. Download the Windows-x64 ZIP release asset.
2. Right-click the downloaded ZIP > Properties. If Windows shows an Unblock
   checkbox, review the source/download and use that checkbox before extracting.
   Do not change your system execution policy or security settings.
3. Extract ALL files to a normal folder. Do not run from inside the ZIP.
4. Double-click "NativeAA-Setup.exe". Browse to the X-Plane 12 root folder
   containing X-Plane.exe, then click Install / update. Any drive, including
   an external SSD, is supported. Setup uses Windows' built-in .NET Framework.
   Read the completion/error message before closing the window.
5. Launch X-Plane with your usual shortcut or third-party launcher. Open
   Plugins > Native AA > Open AA controls. Load a flight and check AA ACTIVE.
The setup executable does not depend on PowerShell scripts. An optional script
interface is also included for advanced users where policy permits it:
  powershell.exe -NoProfile -STA -File .\Install.ps1 -Simulator "D:\X-Plane 12"

SIMULATOR SETTINGS
- Set X-Plane render/input scale to Full (native resolution).
- Keep your normal output resolution and UI size.
- Configure native MSAA in X-Plane Graphics; it may remain enabled. The matched
  showcase used MSAA 2x. Higher MSAA can cost substantially more GPU time/VRAM.
- Turn X-Plane FXAA Off when comparing these modes.
- Do not run another FSR/DLSS/XeSS/NR/FG scene renderer in the same session.
  Launching explicitly through the separate FSR app can prevent Native AA from
  taking over. Native AA does not change that project's saved preferences.

CONTROLS
Use the live On/Off control, select SMAA, FXAA or TAA, then adjust its controls.
Choices save automatically to Output/preferences/NativeAA.ini. No restart is
needed for these AA controls. History weight controls temporal accumulation;
higher values can stabilize static details but can soften or trail moving ones.
Use Reset temporal history after a camera cut or when troubleshooting TAA.
AA ACTIVE requires fresh GPU-completed frames; a saved selection is not proof.
TAA ACTIVE confirms temporal processing specifically. An identified SMAA
fallback means TAA is not currently processing that scene.

ALPHA SCOPE / TROUBLESHOOTING
Tested on Windows, X-Plane 12.4.4 SDR and Radeon AI PRO R9700. Other GPUs and
simulator versions need community validation. Native input up to 4096 pixels
per dimension is admitted; HDR/PQ and distinct multi-view/VR paths are not
validated. Unsupported scene paths bypass rather than claim activity.
TAA uses estimated camera/depth motion, not general native object/cloud vectors.
It may soften textures or show trails. Complete shimmer removal is not claimed.
Additional motion, shimmer and combined-AA work is planned as a later update.
Linux/macOS simulator renderers and installers are not available in this release.

If no activity: verify Full input, normal launcher, plugin enabled and correct
X-Plane folder, then restart. Logs use fresh session filenames in Output/NativeAA.
Include your version/GPU, selected AA mode and current-session log with reports.
The installer leaves resolution, UI scale, controller bindings and camera alone.

UPDATE / DISABLE / UNINSTALL
To update, close X-Plane and run the new package's installer on the SAME root.
It removes obsolete unmodified files from its previous installation inventory.
Unknown or user-modified files are preserved. Keep your preferences if desired.
Live On/Off disables AA processing. To stop ordinary-launch renderer discovery,
close X-Plane and edit Resources/plugins/NativeAA/standalone.enabled so it reads:
  NativeAAStandalone=0
To uninstall, close X-Plane and run "NativeAA-Setup.exe" and click Uninstall, selecting the same
root. It removes only recorded, unchanged Native AA files and its own per-user
Vulkan registration. Other plugins/layer registrations and saved preferences
are preserved. Delete Output/preferences/NativeAA.ini yourself only if you want
to reset your saved choices.

LICENSES AND CONTENT
Original project and setup: MIT. SMAA: MIT. FXAA: BSD / Intel Apache notices. XPLM: SDK
notice. Full notices and referenced source accompany this package/installation.
No NVIDIA runtime/model, AMD/Intel SDK runtime, X-Plane asset, private simulator
shader, capture, preference file or user log is included. The included .dll and
.xpl are built from this project's source. See RELEASE-MANIFEST.json for hashes.

Project / source / comparisons:
https://github.com/cjboy31/make-xp-anti-aliasing-great-again
Community Discord:
https://discord.gg/yh396QNgHZ
