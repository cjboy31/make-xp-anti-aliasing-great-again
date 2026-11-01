<img src="assets/branding/aa-icon-128.png" alt="Native AA icon" width="96">

# Make XP Anti-Aliasing Great Again

Smoother, steadier edges in X-Plane 12 at your normal resolution, adjusted live
from a panel inside the simulator. Works on its own: no FSR, upscaling or frame
generation needed. Also called **Native AA**.

## Download

| Channel | Who it's for | Link |
| --- | --- | --- |
| **Release Candidates** | Everyone. Only tested and verified features. | [Windows 0.3.3](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v0.3.3-alpha) · [Linux 0.3.3 beta](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v0.3.3-linux-beta.1) |
| **Betas** | Try new features early and tell us what you see. | [All releases (Betas are marked *Pre-release*)](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases) |

Mac: not available yet (see [below](#linux-and-mac)).

## Install

**Windows**

1. Close X-Plane. Extract the whole ZIP and run **NativeAA-Setup.exe**.
2. Setup finds X-Plane 12 for you. Check the folder and click **Install / update**.
3. Start X-Plane as usual, load a flight, open **Plugins → Native AA → Open AA controls**.

**Linux (beta)**

1. Close X-Plane. Extract the package and run **`./install.sh`**.
2. It finds X-Plane 12 for you; press **Enter** to install there.
3. Start X-Plane and open **Plugins → Native AA → Open AA controls**.

Point at any **?** in the panel to read what a setting does. Click it to pin the card.

## First settings that work well

- In X-Plane, set the input scale to **Off / Full**. MSAA can stay on; 2x is a good start.
- In the panel, start with **SMAA High**. Flip **On/Off** to compare the same view.
- Want steadier thin lines? Try **TAA + SMAA**, Finishing strength 50%,
  Shimmer reduction 50%, Frame history 85%, History detail **Sharper**.

## What each choice does

| Choice | What to expect |
| --- | --- |
| SMAA | Smooths jagged outlines with little softening. Best starting point. |
| FXAA | Very fast smoothing; can soften fine text and textures. |
| TAA *(experimental)* | Uses recent frames to steady edges and thin lines. Fast-moving objects can leave faint trails. |
| TAA + SMAA / FXAA | TAA, then an extra edge-smoothing step. Uses more GPU time and memory. |
| Motion matching | Helps TAA follow the picture as the camera moves. Movement is estimated. |
| Shimmer reduction | Steadies slow-moving thin lines and distant detail in TAA. |
| History detail *(new in v2, Beta)* | **Sharper** keeps more detail while moving and stops bright lights from flickering. **Classic** is the earlier look. |

Choices save automatically and change live while you fly. No setting removes
every flickering edge, cloud artifact or trail.

### Using the FSR mod too?

Keep both installed if you like. Choose one on the **Renderer** tab and restart
X-Plane; only one processes the picture, and both keep their settings.
[FSR Upscaling + Frame Gen](https://github.com/cjboy31/xplane12-fsr-framegen).

## See the difference

Matched Windows screenshots of the same view. Stills can't show shimmer while moving.
[Capture details](docs/showcase/README.md).

![SMAA comparison](docs/showcase/smaa-high-comparison.png)
![FXAA comparison](docs/showcase/fxaa-comparison.png)
![TAA comparison](docs/showcase/taa-comparison.png)

## Graphics cards

Native AA uses its own standard Vulkan compute shaders, so it is not tied to one
brand. Main test card: AMD Radeon AI PRO R9700. Intel Arc and NVIDIA should work
the same way; reports are welcome. If the panel says **Bypassed**, read the
reason it shows and see the [Zink notes](docs/ZINK-COMPATIBILITY.md).

## Linux and Mac

- **Linux:** a real native beta (Vulkan layer + controls plugin). Automated
  builds and GPU tests pass; flights on Linux are still being tested.
  [Requirements](docs/LINUX-BETA-INSTALLATION.txt).
- **Mac:** X-Plane uses Apple's Metal graphics on Mac, which this mod can't hook
  into yet. There is no Mac download.

## Help

[Settings guide](docs/CONTROLS.md) ·
[Report a problem](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/issues/new?template=bug_report.yml) ·
[Suggest an idea](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/issues/new?template=improvement.yml) ·
[Upload diagnostic logs](https://xplane-mod-log-reports.cjboy31.chatgpt.site/) ·
[Discord](https://discord.gg/yh396QNgHZ) ·
[What we're working on](PIPELINE.md) · [Changelog](CHANGELOG.md)

An optional [shadow script](extras/shadows/README.txt) is included separately and starts Off.

---

**For developers:** [Build and validation](docs/developer/BUILD-AND-VALIDATION.md) ·
[Architecture](docs/developer/ARCHITECTURE.md) · [Motion research](docs/developer/MOTION-SOURCES-20261031.md) ·
[Contributing](CONTRIBUTING.md) · [All developer notes](docs/developer/).
Original code is MIT. [Third-party notices](THIRD_PARTY.md) are included.
