# 0.2.0-alpha - first Windows AA preview

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

[Controls/setup](CONTROLS.md) · [Live TAA validation](TAA-LIVE-VALIDATION.md) ·
[Community Discord](https://discord.gg/yh396QNgHZ)
