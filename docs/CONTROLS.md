# Native AA controls

Open **Plugins > Native AA > Open AA controls**. Choices save automatically.
Use **?** beside a choice for help. **Changelog** shows what changed.

The status strip shows what the renderer is doing now. After a change, wait for
the renderer to receive the selection and finish a new frame. When the renderer
is connected, the footer shows scene size, finished frames and mod GPU memory.

## Start here

Set X-Plane's input scale to **Off / Full**. Keep your normal output resolution
and menu size. Turn X-Plane FXAA Off for a clear comparison. You can keep MSAA
enabled; start with 2x if performance allows.

Try **SMAA High** first. Use the **On/Off** button to compare the same view.

- **SMAA:** smooth jagged outlines without keeping earlier pictures.
- **FXAA:** fast smoothing that can soften text and fine textures.
- **TAA — experimental:** combine recent pictures to steady thin edges. Moving objects can leave trails.
- **TAA + SMAA / FXAA:** add extra edge smoothing after TAA. Start Finishing strength at 50%.
- **AA strength:** lower it to keep more of the original picture.
- **Frame history:** higher values steady detail longer; lower values react faster. Start at 85% with TAA.
- **Shimmer reduction:** start at 50%. More can soften fine detail. Zero turns this extra correction Off.
- **Motion matching:** Camera movement costs less. Camera + image matching adds a detail check and costs more graphics time.
- **Test views:** use these to inspect estimated movement or history. Return to Normal view before flying.
- **Reset TAA history:** clear old pictures if an old image remains after a view change.

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
