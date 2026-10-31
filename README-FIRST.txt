MAKE XP ANTI-ALIASING GREAT AGAIN — 0.3.1-alpha
Windows x64 alpha for X-Plane 12.4.4 / Vulkan

INSTALL
1. Close X-Plane normally. Extract ALL files from the Windows ZIP together.
2. Run NativeAA-Setup.exe and select the X-Plane folder containing X-Plane.exe.
3. Launch X-Plane normally, including through your usual third-party launcher.
4. Open Plugins > Native AA > Open AA controls.

STARTING SETTINGS
Set X-Plane input scale to Off / Full and keep your normal output resolution.
Turn X-Plane FXAA Off for comparison. You may keep MSAA; try 2x to begin.
Start with SMAA High. Use On/Off to compare the same view. Choices save live.
Every choice has a ? help button. Open Changelog to read changes.

AA METHODS
SMAA smooths jagged outlines. FXAA is fast but can soften fine detail.
Experimental TAA steadies edges over time but can leave moving trails.
TAA + SMAA / FXAA adds a second edge-smoothing step. Start Finishing strength
at 50%. Extra smoothing costs graphics time and can soften the picture.
Motion matching helps TAA track the picture. Movement is estimated; it cannot
track every aircraft, propeller or cloud. Start Shimmer reduction at 50% and
Frame history at 85%. No setting removes every flickering edge.
Return any test view to Normal view before flying.

IF YOU ALSO HAVE FSR
Update both mods. Choose one on the Renderer tab, then restart X-Plane.
Only one mod processes the scene. Each keeps its saved choices.
Native AA does not use FSR, upscaling, NR or frame generation.

OPTIONAL SHADOWS
extras/shadows contains make XP shadows great again - enhanced edition.lua
and separate instructions. Install it only if wanted, with FlyWithLua NG.
It starts Off and offers live 2K/4K/8K shadow sizes when supported.
8K uses much more graphics memory. It does not fix every shadow artifact.

UPDATE / UNINSTALL
Close X-Plane, then use the new NativeAA-Setup.exe and select the same simulator.
Use Uninstall in setup to remove recorded files. Saved choices are kept.
The installer leaves your output, menu size, controller and camera settings alone.

HELP AND PLATFORM STATUS
Check for AA ACTIVE or TAA ACTIVE after a flight loads. Read any waiting or
inactive message. Logs for the current session are in Output/NativeAA.
Linux has a separate beta package; real Linux flights still need testing.
Mac rendering is not available. VR, HDR, other views and long flights need
more testing. Do not run a second scene-reconstruction mod at the same time.
Community: https://discord.gg/yh396QNgHZ
Downloads: https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases
Keep all included licenses. No simulator assets or private models are bundled.
