# Zink and a bypassed AA renderer

If the panel says **Bypassed**, the AA effect is not running. X-Plane still
draws its normal picture. **Full** input is the correct setting for Native AA;
it cannot clear every compatibility warning.

For a Windows test, turn **Use Zink plugin bridge** Off in X-Plane's Graphics
settings, close X-Plane normally and reopen it. If AA then works, report the
Zink difference. Other add-ons can depend on Zink for correct or faster drawing,
so keep that setting if turning it Off breaks their screens or performance.
This is a comparison/workaround, not a permanent Zink fix.

Share the full warning, mod/X-Plane/graphics-driver versions, GPU and the newest
`renderer-*.log` in your simulator's `Output/NativeAA` folder. Include X-Plane's
`Log.txt` from the same launch. They help distinguish a bridge issue from a
different renderer, unsupported drawing commands or a scene-size mismatch.

## Development correction and test limits

The previous adapter marked every push-descriptor template call unsafe for
compute state, including templates made only for graphics. The correction tracks
the template's bind point from successful creation until destruction, including
core/KHR lifecycle aliases. Known graphics-only pushes no longer disable AA.
The maintenance6 template and direct-push forms use the same distinction.
Compute pushes, unknown templates, extension chains and secondary execution
still bypass where their state cannot be restored safely. The warning now
identifies the first unsupported path instead of combining all causes.

This change does not filter or alter Zink's shaders, descriptor data or graphics
commands. It does not enable AA on plugin/UI images: native scene, shader,
format, size, single-draw and GPU-ownership checks remain in place.

The Windows GPU fixture reproduces the false bypass with the previous DLL and
passes with the corrected one. It checks identical AA pixels with graphics
templates; unchanged native pixels with compute templates; reset recovery;
core/KHR lifecycle queries; maintenance6 template2/push2; and native compute
pipeline, dynamic descriptor offset and push-constant readback after AA.
Existing spatial, TAA/combined, multi-queue, allocation-failure and missing-shader
fixtures pass with no Vulkan validation messages. This isolates a compatibility
bug; it is **not** a full Windows Zink X-Plane flight validation or proof that
this is every reporter's cause. Linux uses the same adapter and needs its
automated checks and a Linux flight test before claiming the reported case fixed.

[Laminar explains Zink's plugin bridge](https://developer.x-plane.com/2023/02/addressing-plugin-flickering/).
[Khronos defines the template bind point](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorUpdateTemplateCreateInfo.html).
No Mesa or other reference-mod code is copied into this correction.
