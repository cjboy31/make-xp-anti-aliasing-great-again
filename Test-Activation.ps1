param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$VulkanHeaders)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$headers=(Resolve-Path -LiteralPath $VulkanHeaders).Path
$root=Join-Path $PSScriptRoot 'build/activation/sim'
$renderer=Join-Path $root 'Resources/plugins/NativeAA/renderer'
New-Item -ItemType Directory -Path $renderer -Force|Out-Null
$exe=Join-Path $root 'X-Plane.exe'
& $cc -std=c++17 -O2 -static -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers -DWIN32_LEAN_AND_MEAN -DNOMINMAX ('-I'+(Join-Path $headers 'include')) tests/activation_probe_win.cpp build/vulkan/vulkan-loader.lib -o $exe
if($LASTEXITCODE -ne 0){throw 'Activation probe compilation failed'}
$other=Join-Path (Split-Path $root -Parent) 'unrelated-vulkan-app.exe'
Copy-Item -LiteralPath $exe -Destination $other -Force
$dll=Join-Path $PSScriptRoot 'build/live-layer/native-aa-live.dll'
$installedDllName='native-aa-'+(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.Substring(0,12).ToLowerInvariant()+'.dll'
Copy-Item -LiteralPath $dll -Destination (Join-Path $renderer $installedDllName) -Force
. (Join-Path $PSScriptRoot 'tools/LayerManifest.ps1')
[IO.File]::WriteAllText((Join-Path $renderer 'native-aa-live.json'),((New-NativeAALayerManifest $installedDllName)|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
$marker=Join-Path (Split-Path $renderer -Parent) 'standalone.enabled'
[IO.File]::WriteAllText($marker,"NativeAAStandalone=1`n")
Get-ChildItem Env: | Where-Object {$_.Name -like 'NATIVE_AA_*' -or $_.Name -like 'XPFSR24_*' -or $_.Name -eq 'DISABLE_XPFSR24'} | ForEach-Object {Remove-Item -LiteralPath ('Env:'+$_.Name)}
$env:VK_LAYER_PATH=$renderer
$env:VK_INSTANCE_LAYERS='VK_LAYER_NATIVE_AA_live_test'
$env:VK_LOADER_LAYERS_DISABLE='~implicit~'
& $other passive
if($LASTEXITCODE -ne 0){throw 'Unrelated app was not isolated'}
& $exe selected
if($LASTEXITCODE -ne 0){throw 'Installed selected executable did not activate'}
$env:NATIVE_AA_DISABLE='1'
& $exe passive
if($LASTEXITCODE -ne 0){throw 'Opt-out was ignored'}
Remove-Item Env:NATIVE_AA_DISABLE
[IO.File]::WriteAllText($marker,"disabled`n")
& $exe passive
if($LASTEXITCODE -ne 0){throw 'Disabled installation was admitted'}
[IO.File]::WriteAllText($marker,"NativeAAStandalone=1`n")
$env:XPFSR24_AUTOMATIC='1'
& $exe passive
if($LASTEXITCODE -ne 0){throw 'Existing FSR session was not respected'}
$receipt=[ordered]@{status='PASS';dllSHA256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash;manifestSourceSHA256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'tools/LayerManifest.ps1') -Algorithm SHA256).Hash;cases=@('unrelated executable','selected exact executable','environment opt-out','disabled marker','FSR startup conflict')}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'build/live-layer/activation-receipt.json'),($receipt|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
Write-Output 'Activation isolation: all five lifecycle/selection cases passed. No registry or real simulator files changed.'
