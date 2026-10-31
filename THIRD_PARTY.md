# Dependency research

The initial repository contains original code and documentation only. It has
no graphics-runtime, shader, lookup-table, model or X-Plane SDK dependency.
CMake and a compiler are developer tools and are not redistributed here.

## SMAA candidate (not bundled or implemented)

Primary reference: [iryoku/smaa](https://github.com/iryoku/smaa), MIT.
Its reference shader and AreaTex/SearchTex data are candidates for a future
reviewed integration. Pin an exact upstream commit, retain the upstream license
and inventory every imported file before adding them. The present core is not
an implementation of SMAA. No upstream benchmark or screenshot is our result.

## X-Plane interface research

- [12.4.4 SDK update](https://www.x-plane.com/2026/09/whats-new-in-12-4-4-the-sdk-update/)
  describes Panel Graphics and native Dear ImGui rendering hooks. These help
  plugin UI; that announcement is not a contract for reading/replacing the
  simulator's native scene color.
- [Plugin drawing guidance](https://developer.x-plane.com/article/plugin-guidance-for-opengl-drawing/)
  documents synchronization costs and intended plugin callback use. Its older
  OpenGL-only statement predates the newer UI APIs; use current SDK declarations
  when implementing the interface. Do not infer access to internal Metal/Vulkan
  resources from a plugin drawing callback.

No FSR-project source or third-party motion-vector plugin code has been copied
into this repository. Keep any later dependency license separate from the
original-code MIT license and recheck redistribution before packaging.
