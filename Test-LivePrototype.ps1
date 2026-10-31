param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$VulkanHeaders,[Parameter(Mandatory)][string]$Glslang,[Parameter(Mandatory)][string]$Validation)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$headers=(Resolve-Path -LiteralPath $VulkanHeaders).Path
$glsl=(Resolve-Path -LiteralPath $Glslang).Path
$validation=(Resolve-Path -LiteralPath $Validation).Path
$out=Join-Path $PSScriptRoot 'build/live-layer'
$shaders=Join-Path $PSScriptRoot 'build/vulkan/shaders'
[IO.File]::WriteAllText((Join-Path $out 'test-receipt.json'),'{"status":"RUNNING"}')
foreach($entry in @(@('native_scene_test.vert','native-scene.vert.spv'),@('native_scene_test.frag','native-scene.frag.spv'),@('compute_state_test.comp','compute-state.comp.spv'))){
 & $glsl -V --target-env vulkan1.3 (Join-Path $PSScriptRoot ('shaders/'+$entry[0])) -o (Join-Path $shaders $entry[1])
 if($LASTEXITCODE -ne 0){throw 'Native scene test shader failed'}
}
$flags=@('-std=c++17','-O2','-static','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-missing-field-initializers','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX',('-I'+(Join-Path $headers 'include')),('-I'+(Join-Path $PSScriptRoot 'include')),('-I'+(Join-Path $PSScriptRoot 'third_party/smaa')))
& $cc @flags src/vulkan/smaa_pipeline.cpp src/vulkan/compute_smaa_pipeline.cpp src/vulkan/camera_motion_pipeline.cpp src/motion.cpp tests/native_scene_layer_tests.cpp build/vulkan/vulkan-loader.lib -o (Join-Path $out 'native_scene_layer_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Native scene fixture compilation failed'}
$env:VK_LAYER_PATH=$out+';'+$validation
$env:VK_LOADER_LAYERS_DISABLE='~implicit~'
$env:VK_INSTANCE_LAYERS='VK_LAYER_NATIVE_AA_live_test'
$env:NATIVE_AA_EXE=Join-Path $out 'native_scene_layer_tests.exe'
$env:NATIVE_AA_TEST_SHADER=(Get-FileHash -LiteralPath (Join-Path $shaders 'native-scene.frag.spv') -Algorithm SHA256).Hash.ToLowerInvariant()
$env:NATIVE_AA_LIVE_TEST='1'
$env:DISABLE_XPFSR24='1'
$env:NATIVE_AA_SHADERS=$shaders
$env:NATIVE_AA_LOG=Join-Path $out 'fixture.log'
$env:NATIVE_AA_CONTROL=Join-Path $out 'fixture-control.txt'
[IO.File]::WriteAllText($env:NATIVE_AA_LOG,'')
& $env:NATIVE_AA_EXE $shaders
if($LASTEXITCODE -ne 0){throw 'Live adapter fixture failed'}
$lines=Get-Content -LiteralPath $env:NATIVE_AA_LOG
if(-not ($lines | Select-String 'COMPLETED aa_frames=') -or -not ($lines | Select-String 'scratch_remaining=0')){throw 'Missing current-session completion or teardown evidence'}
Write-Output 'Live adapter fixture passed with completion and scratch-retirement evidence.'
$env:NATIVE_AA_LOG=Join-Path $out 'allocation-bypass.log'
[IO.File]::WriteAllText($env:NATIVE_AA_LOG,'')
$env:NATIVE_AA_TEST_FAIL_ALLOC='1'
& $env:NATIVE_AA_EXE $shaders --expect-bypass
if($LASTEXITCODE -ne 0){throw 'Allocation bypass failed'}
Remove-Item Env:NATIVE_AA_TEST_FAIL_ALLOC
if(-not ((Get-Content -LiteralPath $env:NATIVE_AA_LOG)|Select-String 'test allocation refusal')){throw 'Allocation failure was not exercised'}
$env:NATIVE_AA_LOG=Join-Path $out 'shader-bypass.log'
[IO.File]::WriteAllText($env:NATIVE_AA_LOG,'')
$env:NATIVE_AA_SHADERS=Join-Path $out 'deliberately-missing-shaders'
& $env:NATIVE_AA_EXE $shaders --expect-bypass
if($LASTEXITCODE -ne 0){throw 'Shader failure bypass failed'}
if(-not ((Get-Content -LiteralPath $env:NATIVE_AA_LOG)|Select-String 'scratch_remaining=0')){throw 'Failure bypass retained scratch'}
Write-Output 'Allocation and shader failures retained native pixels and retired all layer scratch.'
$receipt=[ordered]@{status='PASS';dllSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'native-aa-live.dll') -Algorithm SHA256).Hash;fixtureSHA256=(Get-FileHash -LiteralPath $env:NATIVE_AA_EXE -Algorithm SHA256).Hash;testedUtc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $out 'test-receipt.json'),($receipt|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
