// SPDX-License-Identifier: MIT
#pragma once
namespace native_aa {
inline const char* option_help(int action) {
    switch(action) {
    case 1:return "Turn the mod on or off while you fly. Off shows X-Plane's image without this mod. Your other choices stay saved.";
    case 10:return "SMAA smooths jagged edges in the current picture. Start here for a clear image and low cost. It cannot stop every flickering line when the view moves.";
    case 11:return "FXAA is a fast edge smoother. It can also soften small text and fine textures. Choose SMAA if the picture looks too soft.";
    case 12:return "TAA combines several recent pictures to steady thin lines and edges. It is experimental. Moving objects can leave trails, so compare it while panning the camera.";
    case 13:return "TAA steadies edges over time, then SMAA smooths the edges that remain. Start with Finishing strength at 50%. This uses more graphics memory and processing time than TAA alone.";
    case 14:return "TAA steadies edges over time, then FXAA adds smoothing. This can help rough outlines, but too much finishing can blur fine detail.";
    case 20:case 21:case 22:case 23:return "SMAA quality controls how carefully edges are checked. High is a good starting point. Lower it if performance drops. This option is used by SMAA and TAA + SMAA.";
    case 30:return "AA strength mixes the smoother picture with the original. 100% uses the full effect. Lower it to keep more original detail. At 0% the effect is off.";
    case 31:return "Fine-detail smoothing controls how much FXAA softens small bright edges. Lower it if cockpit text or textures become soft. This is used by FXAA.";
    case 32:return "Frame history controls how long TAA keeps detail from earlier pictures. Higher values can reduce shimmer. Lower values react faster and can reduce moving trails. Start at 85%.";
    case 33:return "Shimmer reduction steadies slow-moving thin lines and distant detail in TAA. Start at 50%. More is not always better: check moving scenery for softness or trails. Zero turns this extra correction off.";
    case 34:return "Finishing strength controls the extra SMAA or FXAA step after TAA. Start at 50%. Raise it for rough edges or lower it to keep fine textures clear.";
    case 40:return "Restore this mod's default AA choices. Your simulator resolution, controllers and the selected renderer are kept.";
    case 50:return "Forget the pictures saved by TAA and start fresh. Use this if an old image stays visible after changing the view. It does not reset your settings.";
    case 70:return "Use camera movement and scene depth to match earlier pictures. This is the lower-cost choice and a good starting point for TAA.";
    case 71:return "Also compare nearby image details to improve the motion estimate. This costs more graphics time. It estimates movement; it does not know the true movement of every aircraft, propeller or cloud.";
    case 80:return "Show the normal scene. Choose this after checking either motion test view.";
    case 81:return "Replace the scene with colors showing estimated movement. This is a test view, not an AA improvement. Select Normal view to return to flying.";
    case 82:return "Show where TAA trusts older pictures. This is a test view for tracking trails or shimmer. Select Normal view to return to flying.";
    case 90:return "Use this independent AA mod on your next launch. Set X-Plane's input scale to Off / Full. Close and reopen X-Plane to switch renderers.";
    case 91:return "Use the separate FSR mod on your next launch. It must also be installed and updated. Native AA stays inactive while FSR runs. Close and reopen X-Plane after choosing.";
    case 92:return "Use X-Plane without either mod's image processing on the next launch. Both mods keep your saved choices.";
    case 93:return "Choose Native AA when it is installed and enabled; otherwise use FSR when available. Only one mod processes the scene. A renderer change needs a restart.";
    default:return "";
    }
}
inline const char* changelog_text() {
    return "What's new in 0.3.1\n? help explains each choice and suggests a starting point.\nRead changes here instead of watching a startup video.\nThe optional shadow script is installed separately with FlyWithLua.\nTAA + SMAA / FXAA, motion matching and shimmer controls are retained.\nRenderer chooses one mod for your next launch and keeps your settings.\nLinux is a beta; Mac rendering is not available.\nTAA is experimental. Some flickering edges and moving trails may remain.";
}
}
