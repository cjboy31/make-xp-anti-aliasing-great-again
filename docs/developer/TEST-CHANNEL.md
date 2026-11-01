# Private Test channel

Owner-only. Builds are live-tested here before they reach **Betas**, and nothing
reaches **Release Candidates** until it has passed both.

```
test branch ──► draft release test-<version>.<n> ──► live test ──► beta branch / Betas ──► main / Release Candidates
```

## Why it stays private

- Test builds are **draft** GitHub releases, visible only to accounts with write
  access to this repository.
- Tags start with `test-`, never `v`, so the Native AA updater (which only reads
  `vX.Y.Z[-alpha|beta|rc.N]` tags from the public feed) can never offer them.
- The `test` branch is not linked from the README.

## Publishing a test build

1. Merge into `test`. Build Windows (`Build-Plugin.ps1`, `Build-Setup.ps1`,
   `Build-ReleasePackage.ps1`) and/or Linux (`tools/package_linux.py` after the CMake build).
2. `./Publish-TestBuild.ps1 -Version 2.0.0 -Files <zip>,<tar.gz> -Notes '<what changed, what to look at>'`

## Testing a build (ChatGPT or another tester)

1. Open **Releases**, find the newest `TEST … (private)` draft, download, and
   check against `SHA256SUMS.txt`.
2. Close X-Plane. Windows: run `NativeAA-Setup.exe` (it should pre-fill the
   X-Plane folder). Linux: `./install.sh` (press Enter at the detected folder).
3. Start X-Plane, load the **same saved flight each time** (same airport,
   aircraft, time, weather). Open **Plugins > Native AA > Open AA controls**.
4. Run the checks in the release note first, then this baseline:

| # | Check | Pass when |
| --- | --- | --- |
| 1 | Status strip | Shows **AA ACTIVE** (or the TAA variant) within a few seconds; footer counts frames |
| 2 | Point at any **?** | Card fades in beside it; settings stay visible; click pins; click again closes |
| 3 | SMAA High, On/Off on the same view | Edges visibly smoother when On; no black or frozen frames |
| 4 | TAA + SMAA, History detail **Classic** vs **Sharper**, slow 30 s pan over runway lines and trees | Sharper is at least as steady and visibly crisper; no new trails; note GPU time in the footer if shown |
| 5 | Taxi past airport lights at night with Sharper | Bright lights flicker less than Classic |
| 6 | Restart X-Plane | Every choice (including History detail) is still selected |
| 7 | Renderer tab: switch to FSR (if installed), restart, switch back | Only one mod processes the scene; both keep their settings |
| 8 | Uninstall, reinstall | Preferences kept; X-Plane starts normally after uninstall |

5. Collect the newest `Output/NativeAA/renderer-*.log` and X-Plane's `Log.txt`.

## Report template

```
Build: test-<version>.<n>   Commit: <sha>
System: GPU + driver, CPU, OS, resolution + refresh, X-Plane version, Zink on/off
Scene: aircraft, airport, time, weather
Results: 1 PASS/FAIL ... 8 PASS/FAIL (one line each; numbers where asked)
Classic vs Sharper: which looked better and why
Problems: what, when, how to repeat
Logs: attached / link
Verdict: PROMOTE TO BETAS  or  NEEDS FIX
```

Promotion from Betas to Release Candidates additionally needs CI green, the GPU
fixtures on real hardware, and the measured comparison listed in PIPELINE.md.
