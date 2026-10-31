param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$XPlaneSDK)
$ErrorActionPreference='Stop'
$cc=(Resolve-Path -LiteralPath $Compiler).Path
$sdk=(Resolve-Path -LiteralPath $XPlaneSDK).Path
$out=Join-Path $PSScriptRoot 'build/plugin/NativeAA/64'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$flags=@('-std=c++17','-O2','-static','-shared','-Wall','-Wextra','-Wpedantic','-Werror','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX',('-I'+(Join-Path $PSScriptRoot 'include')),('-I'+(Join-Path $sdk 'CHeaders/XPLM')))
& $cc @flags (Join-Path $PSScriptRoot 'src/plugin/native_aa_plugin.cpp') (Join-Path $sdk 'Libraries/Win/XPLM_64.lib') -lopengl32 -o (Join-Path $out 'win.xpl')
if($LASTEXITCODE -ne 0){throw 'Native AA XPLM plugin compilation failed'}
$inputs=@('src/plugin/native_aa_plugin.cpp','include/native_aa/settings.hpp','include/native_aa/live_channel_win.hpp','include/native_aa/scene_camera.hpp')
$inputs+=@('include/native_aa/live_channel.hpp','include/native_aa/platform.hpp')
$receipt=[ordered]@{status='PASS';pluginSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'win.xpl') -Algorithm SHA256).Hash;inputs=@($inputs|ForEach-Object{[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $_) -Algorithm SHA256).Hash}})}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'build/plugin/build-receipt.json'),($receipt|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output 'Built NativeAA/64/win.xpl. No simulator files changed.'
