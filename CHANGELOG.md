# Changes

## 2.0.0-beta.windows.1 (Windows) / 2.0.0-linux-beta.1 (Linux)

- **Sharper TAA history (Beta):** a new *History detail* choice in *Motion & shimmer*.
  *Sharper* keeps more fine detail while the camera moves (Catmull-Rom history
  sampling), reduces colour trails (clipping toward the neighbourhood centre) and
  stops bright lights and glints from flickering (inverse-brightness weighting).
  *Classic* is the 0.3 behaviour. New and existing users start on Sharper.
- **Hover help:** point at any **?** to read help right there. Click it to pin the
  card; click again to close. Help no longer opens a separate page.
- **Easier install:** Windows setup and the Linux installer find X-Plane 12 for
  you (from X-Plane's own `x-plane_install_12.txt`). On Linux, press Enter to accept.
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
R9700. Broader GPU/version compatibility needs testing. TAA remains experimental:
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
