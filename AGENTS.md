# Native AA development and releases

The owner wants an independent native-resolution AA plugin for X-Plane 12.
Keep FSR/NR/frame generation out of this project's processing requirements.
Preserve live controls, saved selections, fresh completion status and normal or
third-party launcher activation. Keep controller/camera bindings unchanged.

## Platform and release preferences

- Include Linux builds/tests and Linux packaging in upcoming updates. Publish
  the first native Linux package as a separately labeled **beta**. Describe
  unsupported features, runtime requirements and testing limits precisely.
- Native Linux rendering needs a Linux plugin, Vulkan layer, IPC and activation;
  CPU/offscreen CI alone is not simulator compatibility. Mac requires a Metal
  adapter. Do not relabel a diagnostic probe or Windows/Wine build as a native port.
- Preserve the published Windows baseline while platform work develops.
- The owner requested the current SMAA/FXAA/experimental TAA baseline released
  before new motion assistance, shimmer and AA-combination changes. Keep those
  follow-up changes on a separate development branch until tested.
- Finish relevant checks, source/license review, versioned package, plain-text
  setup instructions and hash verification before publishing. Preserve prior
  releases/tags and user settings; delete only recognized unchanged owned files.
- Do not ship vendor runtime/model binaries, simulator/native shader assets,
  user configurations, private captures/logs or credentials. Retain notices.
- After release publication/asset verification, routine announcements to the
  community updates channel are authorized:
  https://discord.com/channels/1565487749832777830/1565816821523681290
  Check for duplicates, describe only delivered features, avoid mass mentions,
  and verify the message was sent. Invite: https://discord.gg/yh396QNgHZ
- Maintain the before/after showcase with genuine matched native captures.
  Stills are not proof of moving shimmer, ghosting, pacing or FPS improvements.

## Technical evidence

Read docs/TAA-LIVE-VALIDATION.md, docs/CONTROLS.md and platform-specific validation
notes before rendering changes. Keep exact scene/depth/shader admission, native
state restoration, pending GPU ownership, bounded resource budgets and safe
bypass. A selected mode or plugin presence is not proof of GPU completion.

Run relevant CPU/GPU/activation/installer tests and hosted platform checks.
Recheck against the last published source and existing fixes. Mark unverified
Linux flights, HDR/PQ, VR/multi-view, other GPUs and long-flight behavior as such.
