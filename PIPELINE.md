# Work pipeline

The project's memory: every idea, question, feature request and bug lands here
with a type and a priority. Add one with an
[idea or improvement](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/issues/new?template=improvement.yml)
or a [bug report](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/issues/new?template=bug_report.yml).

## How items are sorted

**Type:** **Feature** (new capability) · **QoL** (easier, clearer, nicer) ·
**Optimization** (less shimmer per millisecond, steadier frame times, less memory) · **Bug**.

| Priority | Rule |
| --- | --- |
| P0 | Bug or regression in the latest Release Candidate or Beta. First. |
| P1 | Directly extends or verifies the most recent update. |
| P2 | New work backed by research or measurements. |
| P3 | Exploratory; needs research or hardware we don't have. |

**Stages:** `Queued → Test (private) → In Beta → Verifying → Release Candidate`.
Shimmer reduction is the owner's top rendering priority, so within a priority
level, anything that measurably reduces moving shimmer goes first.

## Shipped in v2 (waiting for verification)

| Type | Item | Status |
| --- | --- | --- |
| Optimization | **Sharper history**: Catmull-Rom history fetch, AABB clip toward the neighbourhood centre, inverse-luminance blend weights. Classic kept as a choice. | In Beta |
| QoL | Hover help cards (point at **?**, click to pin) | In Beta |
| QoL | Windows setup and Linux installer find X-Plane 12 from `x-plane_install_12.txt` | In Beta |
| QoL | Release notes merged into `CHANGELOG.md`; developer docs moved to `docs/developer/` | In Beta |

## Queued

### Optimization

| P | Item | Why / evidence | Notes |
| --- | --- | --- | --- |
| P1 | Measure Sharper vs Classic on one camera path: temporal flicker, retained detail, ghosting, GPU ms | Needed before promoting Sharper to Release Candidates | `tools/render_evidence.py`; fences, runway lines, trees, cockpit text |
| P2 | CMAA 2 as a spatial mode | Intel's conservative morphological AA: sharper than FXAA, cheaper than SMAA, plain compute that runs on every vendor | Apache-2.0 source from the same upstream already vendored for FXAA; port HLSL to GLSL with hash-pinned provenance |
| P2 | SMAA T2x-style mode: jittered SMAA + 2-frame temporal resolve | Well-known way to cut shimmer at low cost; reuses our SMAA and TAA jitter | Compare against TAA + SMAA |
| P2 | Depth-dilated motion (closest depth in 3×3) and velocity-weighted feedback | Reduces edge ghosting on moving silhouettes | Small shader change; fixture first |
| P2 | Zink matrix: run with X-Plane `--zink` and `--no_zink` | Laminar plans Zink as the only plugin GL path | Builds on the 0.3.3 push-descriptor fix |
| P3 | Batch panel drawing (vertex arrays instead of per-quad `glBegin`) | Panel GL calls are translated to Vulkan by Zink; fewer calls = less overhead while the panel is open | Visual output must not change |

### Feature

| P | Item | Why / evidence | Notes |
| --- | --- | --- | --- |
| P2 | Real per-object motion vectors shared with the FSR mod | X-Plane has no velocity buffer; only patching sim shaders at compile time gives true object motion | The community *Motion Vectors for X-Plane 12 V2* is GPL-3.0 and its own renderer, so it can't be dropped into this MIT mod. Needs an original implementation with one producer and both mods consuming |
| P3 | Optional contrast-adaptive sharpening after TAA | Some users prefer crisper output | Off by default; never a substitute for detail |
| P3 | Mac: Metal / MetalFX backend | X-Plane on Mac uses Metal, so the Vulkan layer never sees its frames | A separate project-sized effort |

### QoL

| P | Item | Notes |
| --- | --- | --- |
| P1 | Make the Native AA updater channel-aware (Release Candidates / Betas) | v2 Windows Beta uses the `beta.windows.N` tag so today's updater doesn't offer it to everyone |
| P1 | Linux: optional `zenity` dialog when Tk is missing | Keeps the one-click path on minimal desktops |
| P2 | Scrollable Changelog tab | The list is growing |

### Bugs

None open in this file. Live bugs: [issues labelled pipeline: bug](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/issues?q=label%3A%22pipeline%3A+bug%22).

## Research notes behind this list

- **LuminaXP** fixes AA on NVIDIA by feeding DLAA/DLSS real jitter and depth; we can't use NVIDIA's runtime on AMD/Intel, so the takeaways are behavioral: jitter only geometry (not sky/clouds), reject history on camera cuts, keep a spatial fallback.
- **ReShade** has known crashes with X-Plane 12 Vulkan when an aircraft loads; not a dependency.
- **Zink** turns plugin OpenGL into Vulkan on X-Plane's own device, which is why its descriptor templates reach our layer.
- Standard TAA quality techniques adopted in v2 follow Karis (2014) and Jimenez's 5-tap Catmull-Rom history filter; all code is original.
