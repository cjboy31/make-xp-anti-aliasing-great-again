# Make XP Anti-Aliasing Great Again

![Native AA icon](assets/branding/aa-icon-256.png)

**Windows Native AA is now Pure inside Aerolux.** The separate Windows mod and updater are retired. Pure smooths edges at native resolution; you do not have to turn on FSR, neural rendering or frame generation.

## Current Windows download

**[Download Aerolux 2.0 Release Candidate 1](https://github.com/cjboy31/xplane12-fsr-framegen/releases/download/v2.0.0-rc.1/XPlane-FSR3-4-FrameGen-2.0.0-rc.1-Setup.exe)** · [Changes, checksums and limits](https://github.com/cjboy31/xplane12-fsr-framegen/releases/tag/v2.0.0-rc.1)

1. Close X-Plane and the old updater. Run the RC installer once; keep the existing app folder when updating. Do not uninstall first.
2. Select your X-Plane folder. Setup keeps saved AA choices and only removes unchanged standalone files it owns. Edited and unknown files are protected.
3. Start X-Plane normally. Open **Plugins > Aerolux > Open settings** and choose **Pure**.
4. Use Full input resolution. Begin with SMAA and compare On with Off. Use **?** for help. In Manager > Updates, choose **Release Candidates** for RC updates.

This is a Release Candidate for final checks, not a final release. [Aerolux Beta 9 remains archived for recovery](https://github.com/cjboy31/xplane12-fsr-framegen/releases/tag/v2.0.0-beta.windows.9).

## Existing Linux preview

[Native AA 2.0 Linux Beta 3](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v2.0.0-linux-beta.3) · [Linux installation help](docs/LINUX-BETA-INSTALLATION.txt) · [Platform limits](docs/PLATFORM-STATUS.md)

This separate Linux Beta stays available with its existing features and limits. The Windows RC does not merge Linux Pure, NR or FG support, and hosted tests do not prove Linux X-Plane flight quality.

## AA features and limits

- SMAA, FXAA and CMAA 2 to smooth edges.
- Experimental TAA and combined modes to steady edges over time.
- Motion/history controls, optional sharpening and an AA memory budget.
- Saved settings, ? help, diagnostics and an in-sim changelog.

Temporal modes can leave trails or soften detail. Higher settings cost GPU time. No mode removes every shimmer. [The standalone controls guide](docs/CONTROLS.md) applies to older packages; current Windows controls are in Aerolux.

## Older standalone Windows packages

[Earlier downloads](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases) stay available for recovery. Follow the instructions inside the matching older package. The [0.3.4 Alpha Zink hotfix](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases/tag/v0.3.4-alpha) is an older manual hotfix, not the current Windows download.

## Before and after

Matched screenshots from the initial preview show the edge-smoothing effect. Stills do not prove moving-image quality or performance.

![SMAA before and after](docs/showcase/smaa-high-comparison.png)

[View FXAA and TAA comparisons](docs/showcase/README.md)

## Help and licensing

[Join Discord](https://discord.gg/yh396QNgHZ) · [All downloads](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/releases) · [Changelog](CHANGELOG.md) · [Licensing](LICENSING.md)

Choose the Windows ZIP or Linux tar.gz under Assets. GitHub's automatic “Source code” archives contain this download page, not the plugin's development source. Previously released packages keep their existing licenses. The Windows ZIPs have been repacked without development source; their renderer, plugin and compiled shaders are unchanged.