param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$VulkanHeaders,[Parameter(Mandatory)][string]$Glslang,[Parameter(Mandatory)][string]$Validation)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$headers=(Resolve-Path -LiteralPath $VulkanHeaders).Path
$glsl=(Resolve-Path -LiteralPath $Glslang).Path
$out=Join-Path $PSScriptRoot 'build/temporal'
New-Item -ItemType Directory -Path $out -Force|Out-Null
& $glsl -V --target-env vulkan1.1 shaders/temporal_resolve.comp -o (Join-Path $out 'temporal-resolve.comp.spv')
if($LASTEXITCODE -ne 0){throw 'Temporal shader compilation failed'}
$flags=@('-std=c++17','-O2','-static','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-missing-field-initializers','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX',('-I'+(Join-Path $headers 'include')),('-I'+(Join-Path $PSScriptRoot 'include')),('-I'+(Join-Path $PSScriptRoot 'third_party/smaa')))
& $cc @flags src/vulkan/smaa_pipeline.cpp src/vulkan/compute_smaa_pipeline.cpp src/vulkan/camera_motion_pipeline.cpp src/vulkan/temporal_resolve_pipeline.cpp src/motion.cpp tests/temporal_resolve_gpu_tests.cpp build/vulkan/vulkan-loader.lib -o (Join-Path $out 'temporal_resolve_gpu_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Temporal fixture compilation failed'}
$env:VK_LAYER_PATH=(Resolve-Path -LiteralPath $Validation).Path
$env:VK_LOADER_LAYERS_DISABLE='~implicit~'
Remove-Item Env:VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue
& (Join-Path $out 'temporal_resolve_gpu_tests.exe') (Join-Path $out 'temporal-resolve.comp.spv')
if($LASTEXITCODE -ne 0){throw 'Temporal GPU fixture failed'}
