# Changes

## 2.0.0-beta.windows.1 (Windows) / 2.0.0-linux-beta.1 (Linux)

First v2 Beta of Native AA (owner-approved, 2026-11-02). Choices marked experimental start Off. Checked live on one Windows PC; Linux is built and tested automatically but not yet flown. Mac rendering is not available.

- **AA memory budget (new, under Renderer):** *Auto* checks how much graphics memory is free before adding AA buffers and leaves room for X-Plane. *GPU-size limit* keeps the earlier fixed method. If memory runs short, Auto waits and tries again when room comes back. It does not cap X-Plane's own memory use. Restart after switching.
- **Fix: TAA no longer falls back to SMAA at 4K.** With sharpening on, or after trying CMAA 2, Native AA could run out of its private memory allowance at 4K and switch to plain SMAA until restart. The allowance now grows with your graphics card's memory (from 384 MiB up to 1.5 GiB), and changing any AA setting retries TAA straight away.
- **Fix: Vulkan panel drawing lines up with the window,** and the version text in the header no longer touches the On switch.
- **Switch AA methods from a key or joystick button:** two new X-Plane commands, *Native AA: next AA method* and *Native AA: previous AA method* (`native_aa/next_method`, `native_aa/previous_method`). Bind them in X-Plane's Keyboard or Joystick settings to compare methods live. *Toggle* and *Open controls* were already there.
- **Native motion with MSAA on X-Plane 12.4.4:** with MSAA on, X-Plane 12.4.4 changed how it stores its motion data. *Motion source: Native* now reads that form too.
- **Version shown everywhere:** the panel header and title bar, Plugin Admin, X-Plane's Log.txt and the renderer log now show the exact version and build (for example `2.0.0-beta.windows.1 / build 1a2b3c4`), so reports say which build ran.
- **Fix: Native AA now works with Zink.** With Zink on, X-Plane turns on graphics features that Zink uses for plugin drawing. Native AA used to see those and switch itself off for the whole session. It now only stays away from Zink's own drawing and keeps anti-aliasing X-Plane's picture.
- **Vulkan panel drawing (experimental, Off by default):** on X-Plane 12.4.4 or newer, *Switch panel drawing (OpenGL / Vulkan)* in the Native AA plugin menu lets X-Plane draw this panel itself with its new Panel Graphics instead of the slower OpenGL bridge. The same menu item switches back. Older X-Plane versions keep OpenGL.
- **Texture detail:** new buttons under *AA modes* for the TAA methods. *Sharper* and *Sharpest* make runway, ground and building textures crisper (TAA smooths the extra detail). Applies after restart.
- **Sharpening (off by default):** a new slider in *Motion & shimmer* makes the TAA picture a little crisper. It never pushes a pixel past its neighbours, so it adds no halos. Start around 20 to 30%.
- **CMAA 2:** a seventh AA method, Intel's careful edge smoother. It only touches clear jagged edges, so cockpit text and fine textures stay sharp, and it costs less than SMAA High. The method buttons now sit in two rows.
- **SMAA T2x (experimental):** a sixth AA method. It blends only the current and previous picture (taken from two slightly different spots), then smooths edges with SMAA. It aims to be sharper with fewer trails than TAA; it steadies thin lines a little less. Measure it with check 13 against TAA + SMAA.
- **Mod CPU time in the footer:** *MOD CPU / FRAME* shows how long this mod adds to each frame on X-Plane's render thread (2 s average).
- **Esc closes a pinned help card** (it no longer reaches X-Plane while a card is pinned).
- **Motion source (experimental):** a new choice in *Motion & shimmer*.
  *Native* reads the motion X-Plane works out for its own reflections. TAA then
  follows your own aircraft instead of only the camera, so the cockpit and
  airframe can keep more detail when you turn. It changes live. Pixels Native
  cannot cover fall back to Camera. *Camera* stays the default. It uses the same
  shared motion format and native shader as the FSR mod.

- **Sharper TAA history (Beta):** a new *History detail* choice in *Motion & shimmer*.
  *Sharper* keeps more fine detail while the camera moves (Catmull-Rom history
  sampling), reduces colour trails (clipping toward the neighbourhood centre) and
  is designed to reduce flicker from bright lights and glints (inverse-brightness
  weighting); the night-light measurement is still pending.
  *Classic* is the 0.3 behaviour. New and existing users start on Sharper.
  Sharper also tracks moving edges using the closest depth around each pixel,
  which reduces ghost outlines behind aircraft and buildings as the view moves.
- **Hover help:** point at any **?** to read help right there. Click it to pin the
  card; click again to close. Help no longer opens a separate page.
- **Easier install:** Windows setup and the Linux installer find X-Plane 12 for
  you (from X-Plane's own `x-plane_install_12.txt`). On Linux, press Enter to accept.
- **Scrollable changelog:** use the mouse wheel on the Changelog tab to read older versions.
- **Linux setup without Tk:** the installer now uses the desktop's own dialogs (zenity or kdialog) when Python's Tk is missing.
- Taller panel to fit the new section; release notes merged into this file.
- Not yet verified in live flights; this is why it is a Beta.

## 0.3.3

This update fixes a case where normal graphics work from the Zink plugin bridge
could make Native AA stop processing. It also gives a more specific reason when
a drawing path cannot be handled safely. The picture stays unchanged while AA
is bypassed.

Keep using **Plugins > Native AA > Open AA controls**. Choose Native AA on
**Renderer**, close X-Plane normally and reopen it if you changed that selection.
Use **Full** input scale. Full is needed for Native AA, but cannot clear a
compatibility warning on its own. Both FSR and Native AA can remain installed;
choose one image processor per session.

If Zink still prevents AA from becoming Active, share the exact warning and the
newest log in **X-Plane > Output > NativeAA**, plus X-Plane's **Log.txt** from the
same launch. The [report page](https://xplane-mod-log-reports.cjboy31.chatgpt.site/)
lets you preview filtered copies before sending them to Discord. Enter the
version shown in the Changelog tab. Native AA does not automatically upload logs.

As a comparison, you can turn **Use Zink plugin bridge** Off in X-Plane Graphics,
then close and reopen the sim. Some add-ons need Zink, so restore it if their
screens or performance get worse.

The correction passes Windows GPU checks and Linux automated checks. It does not
prove every Zink/add-on combination is fixed. A full Windows Zink flight and
Linux flight checks still need validation. Linux remains a separately labeled
beta. Mac rendering is unavailable. No new AA algorithm or FPS gain is claimed.

Close X-Plane before updating. Extract the complete Windows ZIP and run
**NativeAA-Setup.exe**, or extract the full Linux beta and run **./install.sh**.
Select your own simulator folder. Saved choices are kept.

## 0.3.2

Open **Plugins > Native AA > Open AA controls**. The smaller panel has four
tabs for AA methods, motion and shimmer, the next-launch renderer, and changes.
Select **?** beside a setting for help.

The panel's edge mark, method indicator, power switch and tab marker move briefly
when you open the panel or change a choice. On Windows, the panel follows the
system setting for UI animations.

The status strip reports whether the renderer is processing the current choice.
It waits for the renderer to acknowledge a change and finish a new frame before
showing **Active**. The footer shows scene size, finished frames and mod GPU
memory when a current renderer report is available.

Install the Windows x64 alpha with **NativeAA-Setup.exe**, or use **install.sh**
from the separate Linux x86_64 beta. Close X-Plane before updating. The setup
keeps saved choices. The Linux beta needs flight testing on native Linux
hardware. See [controls](docs/CONTROLS.md) for flight setup and
[renderer selection](docs/developer/RENDERER-SELECTION.md) for the next-launch choices.

## 0.3.1

- Clearer setting names and a **?** help button beside each AA choice.
- **Changelog** inside X-Plane lets you read changes without a startup video.
- Optional **make XP shadows great again - enhanced edition** Lua script,
  installed separately with FlyWithLua. It starts Off and offers live 2K/4K/8K
  shadow sizes when X-Plane provides the needed control.
- SMAA, FXAA, experimental TAA, combined modes, motion matching and shimmer
  controls are retained. This is a controls and setup update, not a new AA
  algorithm or a promise that all shimmer is fixed.

Windows alpha and native Linux beta packages have separate installation steps.
Linux flight testing is still needed. The optional shadow script's live control
was checked on Windows 12.4.4; Linux/Mac shadow flights have not been verified.

## 0.2.0

Native-resolution SMAA, FXAA and experimental TAA with live controls inside
X-Plane. No FSR, upscaling or frame generation dependency.

- SMAA 1x Low / Medium / High / Ultra.
- FXAA 3.11 with blend and subpixel controls.
- Experimental TAA with geometry jitter, camera/depth reprojection, clipped
  history, adjustable history weight and live reset.
- Live On/Off, automatically saved selections and fresh GPU-completion status.
- Installation into your selected simulator, including external drives, with
  normal/third-party launcher discovery and an inventory-based uninstaller.
- Matched before/after screenshots on the repository home page.

Download **Make-XP-AA-0.2.0-alpha-Windows-x64.zip** under Assets. GitHub's
automatic source archives do not contain the prebuilt plugin. Extract all files,
read README-FIRST.txt, close X-Plane and run NativeAA-Setup.exe.
Open Plugins > Native AA > Open AA controls. Set render input to Full, native
FXAA Off, and avoid another scene reconstruction renderer in that session.
Native MSAA can remain enabled; the showcase used 2x.

**Windows x64 alpha.** Live checks cover X-Plane 12.4.4 SDR on Radeon AI PRO
test GPU. Broader GPU/version compatibility needs testing. TAA remains experimental:
it can soften detail or trail moving content, and camera/depth estimates do not
provide general object/cloud motion. Unsupported paths bypass visibly. HDR/PQ,
VR/distinct multi-view, Linux and macOS simulator rendering are not validated.

The renderer and control plugin are the previously live-tested binaries. Release
preparation adds packaging, setup/uninstall instructions and version metadata.
Their SHA-256 hashes are recorded in the package manifest. The package was
checked for matching source/build/test receipts and redistribution notices.

The new motion-vector assistance, dedicated shimmer improvements and combined-AA
modes are a separate development update; they are **not included here**.

[Controls/setup](docs/CONTROLS.md) · [Live TAA validation](docs/developer/TAA-LIVE-VALIDATION.md) ·
[Community Discord](https://discord.gg/yh396QNgHZ)
