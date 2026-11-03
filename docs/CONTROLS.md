# Native AA controls

Open **Plugins > Native AA > Open AA controls**. Choices save automatically.
Use **?** beside a choice for help. **Changelog** shows what changed.

The status strip shows what the renderer is doing now. After a change, wait for
the renderer to receive the selection and finish a new frame. When the renderer
is connected, the footer shows scene size, finished frames, mod GPU memory and
mod CPU time per frame.

## Start here

Set X-Plane's input scale to **Off / Full**. Keep your normal output resolution
and menu size. Turn X-Plane FXAA Off for a clear comparison. You can keep MSAA
enabled; start with 2x if performance allows.

Try **SMAA High** first. Use the **On/Off** button to compare the same view.

- **SMAA:** smooth jagged outlines without keeping earlier pictures.
- **FXAA:** fast smoothing that can soften text and fine textures.
- **TAA — experimental:** combine recent pictures to steady thin edges. Moving objects can leave trails.
- **TAA + SMAA / FXAA:** add extra edge smoothing after TAA. Start Finishing strength at 50%.
- **CMAA 2:** Intel's careful edge smoother. It only touches clear jagged edges, so text and fine textures stay sharp. Cheaper than SMAA High. AA strength 0 turns it off; other values do not change it.
- **SMAA T2x — experimental:** blends only this picture and the last one (taken from two slightly different spots), then SMAA. Sharper and fewer trails than TAA, but thin lines are steadied a little less. Frame history does not apply.
- **AA strength:** lower it to keep more of the original picture.
- **Frame history:** higher values steady detail longer; lower values react faster. Start at 85% with TAA.
- **Texture detail (TAA, after restart):** X-Plane default, Sharper or Sharpest. Crisper ground and building textures; use X-Plane default with SMAA, FXAA or CMAA 2.
- **Sharpening:** off at 0%. Makes the TAA picture a little crisper; start around 20 to 30%. Too much can make shimmer easier to see.
- **Shimmer reduction:** start at 50%. More can soften fine detail. Zero turns this extra correction Off.
- **Motion matching:** Camera movement costs less. Camera + image matching adds a detail check and costs more graphics time.
- **Motion source (experimental):** Camera is the normal choice. Native uses the motion X-Plane works out for its own reflections, so your aircraft can keep more detail when you turn. It changes live. Pixels it cannot cover use Camera. It does not cover other traffic, propellers or clouds yet.
- **Test views:** use these to inspect estimated movement or history. Return to Normal view before flying.
- **Reset TAA history:** clear old pictures if an old image remains after a view change.
- **AA memory budget (Renderer tab, after restart):** Auto checks available
  GPU memory before adding AA buffers and leaves space for X-Plane. GPU-size
  limit keeps the earlier method. If the driver cannot report headroom, Auto
  uses that fixed limit and says so in the renderer log. This limits the mod's
  extra buffers; it does not cap X-Plane's total graphics memory use.

AA settings work while you fly. **Renderer** chooses Native AA, FSR or the
original simulator for the next launch. Close and reopen X-Plane to switch.
Older Automatic choices still work. The panel shows which renderer that choice
will select. Both mods keep your saved choices.

The status should say **AA ACTIVE** or **TAA ACTIVE** after loading a supported
flight. A selected button alone does not mean it is working. Read any waiting
or inactive message. No method removes every trail or flickering edge.

If Zink changes whether AA works, see the [Zink and bypass troubleshooting steps](ZINK-COMPATIBILITY.md).

## Install, update or remove

Windows: extract the complete ZIP, close X-Plane and run **NativeAA-Setup.exe**.
Choose the X-Plane folder. Run the newer setup in the same folder to update.
Use Uninstall in setup to remove its files while keeping saved choices.

Linux: use the separate beta package and [Linux instructions](LINUX-BETA-INSTALLATION.txt).
Mac rendering is not available. Use your usual simulator or third-party launcher
after setup; no desktop dashboard needs to stay open.

The optional [shadow script](../extras/shadows/README.txt) is installed separately
with FlyWithLua. It starts Off. It does not require either AA renderer.

[Discord help](https://discord.gg/yh396QNgHZ) ·
[Developer build and validation](https://github.com/cjboy31/make-xp-anti-aliasing-great-again/blob/main/docs/BUILD-AND-VALIDATION.md)
