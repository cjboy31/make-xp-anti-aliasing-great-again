# Optional live shadow script

The separately installed script is **make XP shadows great again - enhanced
edition.lua**. It starts Off, uses FlyWithLua NG, and is independent of either
AA renderer. No third-party script was copied.

The installed 12.4.4 rendering table references `fbo/shadow_cam_size` for shadow
map size. This is not proof of a public supported API. Laminar documents
[private art-control limitations](https://developer.x-plane.com/tools/datarefeditor/)
and [why they change](https://developer.x-plane.com/2014/05/art-controls-are-an-active-volcano/).

On the Windows 12.4.4 live test, the documented local Web API reported
`sim/private/controls/fbo/shadow_cam_size` as writable float. Its original value
was 2048. A 4096 write was still readable two seconds later, then the exact 2048
baseline was restored in a finally block. This confirms accepted live dataref
changes, not a measured reduction in moving shadow aliasing or proof that every
renderer recreates every shadow allocation immediately. Linux/Mac flights have
not been checked. Do not replace these limits with an unsupported guarantee.

The Lua uses the XPLM functions directly after checking symbol availability,
dataref existence, writability and scalar type. Writes occur only on a selected
macro, not every frame. Only 2048/4096/8192 are accepted. Off/exit restores the
captured baseline only if the current setting still equals our last write. If
another add-on changes it, that add-on's value wins. It does not edit settings.txt,
cascade distances, weather, camera, controllers or either mod's preferences.

Lua policy tests cover default Off, bounded values, float/int access, restore,
exit, competing writers and missing/read-only/wrong-type/rejected controls. The
test runner is a build-only dependency; no Lua or FlyWithLua runtime is bundled.
