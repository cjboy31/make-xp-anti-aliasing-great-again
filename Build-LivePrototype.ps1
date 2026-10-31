param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$VulkanHeaders)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$headers=(Resolve-Path -LiteralPath $VulkanHeaders).Path
$out=Join-Path $PSScriptRoot 'build/live-layer'
New-Item -ItemType Directory -Path $out -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $out 'build-receipt.json'),'{"status":"BUILDING"}')
$flags=@('-std=c++17','-O2','-static','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-missing-field-initializers','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX','-DVK_NO_PROTOTYPES',('-I'+(Join-Path $headers 'include')),('-I'+(Join-Path $PSScriptRoot 'include')),('-I'+(Join-Path $PSScriptRoot 'third_party/smaa')))
& $cc @flags -include (Join-Path $PSScriptRoot 'src/vulkan/live_pipeline_dispatch.hpp') -c (Join-Path $PSScriptRoot 'src/vulkan/compute_smaa_pipeline.cpp') -o (Join-Path $out 'pipeline.o')
if($LASTEXITCODE -ne 0){throw 'Live recorder compilation failed'}
$temporalObjects=@()
foreach($entry in @('native_taa_pipeline','native_depth_pipeline')){
 $object=Join-Path $out ($entry+'.o')
 & $cc @flags -include (Join-Path $PSScriptRoot 'src/vulkan/live_pipeline_dispatch.hpp') -c (Join-Path $PSScriptRoot ('src/vulkan/'+$entry+'.cpp')) -o $object
 if($LASTEXITCODE -ne 0){throw 'Native TAA recorder compilation failed'}
 $temporalObjects+=$object
}
& $cc @flags -shared (Join-Path $PSScriptRoot 'src/vulkan/live_layer_win.cpp') (Join-Path $out 'pipeline.o') @temporalObjects -lbcrypt -o (Join-Path $out 'native-aa-live.dll')
if($LASTEXITCODE -ne 0){throw 'Live adapter compilation failed'}
$manifest=[ordered]@{
 file_format_version='1.2.0'; layers=@(@{
  name='VK_LAYER_NATIVE_AA_live_test';type='GLOBAL';library_path='.\native-aa-live.dll';api_version='1.3.0';implementation_version='1'
  description='Private opt-in native-resolution SMAA live prototype; unsupported paths bypass'
  functions=@{vkNegotiateLoaderLayerInterfaceVersion='aaNegotiate'}
 })
}
[IO.File]::WriteAllText((Join-Path $out 'native-aa-live.json'),($manifest|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
$inputs=@('src/vulkan/live_layer_win.cpp','src/vulkan/compute_smaa_pipeline.cpp','src/vulkan/live_dispatch.hpp','src/vulkan/live_pipeline_dispatch.hpp','include/native_aa/compute_smaa_pipeline.hpp','include/native_aa/descriptor_state.hpp','third_party/smaa/Textures/AreaTex.h','third_party/smaa/Textures/SearchTex.h','shaders/smaa.comp','build/vulkan/shaders/smaa-0.comp.spv','build/vulkan/shaders/smaa-1.comp.spv','build/vulkan/shaders/smaa-2.comp.spv')
$inputs+=@('include/native_aa/settings.hpp','include/native_aa/live_channel_win.hpp','shaders/fxaa.comp','third_party/fxaa/Fxaa3_11.h')
$inputs+=@('include/native_aa/live_channel.hpp','include/native_aa/platform.hpp')
$inputs+=@('src/vulkan/native_taa_pipeline.cpp','src/vulkan/native_depth_pipeline.cpp','include/native_aa/native_taa_pipeline.hpp','include/native_aa/scene_camera.hpp','shaders/native_taa.comp','shaders/native_depth.comp')
$inputs+=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'build/vulkan/shaders') -File | Where-Object {$_.Name -match '^(live-smaa-[0-3]-[0-2]|fxaa-[02]|native-taa|native-depth-(1|ms))\.comp\.spv$'} | ForEach-Object {'build/vulkan/shaders/'+$_.Name})
$hashes=@($inputs|ForEach-Object{[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $_) -Algorithm SHA256).Hash}})
$receipt=[ordered]@{status='PASS';dllSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'native-aa-live.dll') -Algorithm SHA256).Hash;inputs=$hashes;builtUtc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $out 'build-receipt.json'),($receipt|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output 'Built explicit live-test layer. No simulator files or system activation were changed.'
