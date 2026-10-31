param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$VulkanHeaders,[Parameter(Mandatory)][string]$Glslang,[Parameter(Mandatory)][string]$Validation)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$headers=(Resolve-Path -LiteralPath $VulkanHeaders).Path
$glsl=(Resolve-Path -LiteralPath $Glslang).Path
$out=Join-Path $PSScriptRoot 'build/native-taa'
$shaders=Join-Path $PSScriptRoot 'build/vulkan/shaders'
New-Item -ItemType Directory -Path $out -Force|Out-Null
foreach($entry in @(@('native_taa.comp','native-taa.comp.spv'),@('native_depth.comp','native-depth-1.comp.spv'))){
 & $glsl -V --target-env vulkan1.1 ('shaders/'+$entry[0]) -o (Join-Path $shaders $entry[1])
 if($LASTEXITCODE -ne 0){throw 'Native TAA shader failed'}
}
& $glsl -V --target-env vulkan1.1 -DDEPTH_MSAA=1 shaders/native_depth.comp -o (Join-Path $shaders 'native-depth-ms.comp.spv')
if($LASTEXITCODE -ne 0){throw 'Native MSAA depth shader failed'}
$flags=@('-std=c++17','-O2','-static','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-missing-field-initializers','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX',('-I'+(Join-Path $headers 'include')),('-I'+(Join-Path $PSScriptRoot 'include')),('-I'+(Join-Path $PSScriptRoot 'third_party/smaa')))
& $cc @flags tests/scene_camera_tests.cpp -o (Join-Path $out 'scene_camera_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Camera fixture compilation failed'}
& (Join-Path $out 'scene_camera_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Camera fixture failed'}
& $cc @flags src/vulkan/smaa_pipeline.cpp src/vulkan/compute_smaa_pipeline.cpp src/vulkan/camera_motion_pipeline.cpp src/vulkan/native_taa_pipeline.cpp src/vulkan/native_depth_pipeline.cpp src/motion.cpp tests/native_taa_gpu_tests.cpp build/vulkan/vulkan-loader.lib -o (Join-Path $out 'native_taa_gpu_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Native TAA fixture compilation failed'}
$env:VK_LAYER_PATH=(Resolve-Path -LiteralPath $Validation).Path
$env:VK_LOADER_LAYERS_DISABLE='~implicit~'
Remove-Item Env:VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue
& (Join-Path $out 'native_taa_gpu_tests.exe') $shaders
if($LASTEXITCODE -ne 0){throw 'Native TAA GPU fixture failed'}
