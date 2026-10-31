param([string]$OutputDirectory=(Join-Path $PSScriptRoot 'dist/release-0.3.0-alpha'))
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
$name='Make-XP-AA-0.3.0-alpha-Windows-x64'
$stage=Join-Path $output $name
$zip=Join-Path $output ($name+'.zip')
if((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $zip)){throw 'Output already exists; use a new OutputDirectory to retain previous artifacts'}
$build=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/live-layer/build-receipt.json') -Raw|ConvertFrom-Json
$ui=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/plugin/build-receipt.json') -Raw|ConvertFrom-Json
$test=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/live-layer/test-receipt.json') -Raw|ConvertFrom-Json
$activation=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/live-layer/activation-receipt.json') -Raw|ConvertFrom-Json
$setup=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/setup/build-receipt.json') -Raw|ConvertFrom-Json
if($setup.status -ne 'PASS' -or (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'build/setup/NativeAA-Setup.exe') -Algorithm SHA256).Hash -ne $setup.setupSHA256){throw 'Setup executable requires a matching build receipt'}
$dll=Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'build/live-layer/native-aa-live.dll') -Algorithm SHA256
$plugin=Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'build/plugin/NativeAA/64/win.xpl') -Algorithm SHA256
if($build.status -ne 'PASS' -or $ui.status -ne 'PASS' -or $test.status -ne 'PASS' -or $activation.status -ne 'PASS' -or $dll.Hash -ne $build.dllSHA256 -or $dll.Hash -ne $test.dllSHA256 -or $dll.Hash -ne $activation.dllSHA256 -or $plugin.Hash -ne $ui.pluginSHA256){throw 'Only matching checked binaries may be packaged'}
if($activation.manifestSourceSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'tools/LayerManifest.ps1') -Algorithm SHA256).Hash){throw 'Activation manifest changed since validation'}
$paths=@('NativeAA-Setup.exe','build/setup/build-receipt.json','Install.ps1','Uninstall.ps1','Install-DevelopmentBuild.ps1','tools/Select-Simulator.ps1','tools/LayerManifest.ps1','README-FIRST.txt','LICENSE','THIRD_PARTY.md','docs/CONTROLS.md','docs/RELEASE-0.2.0.md','docs/TAA-LIVE-VALIDATION.md','docs/MOTION-SHIMMER-COMBINATIONS.md','third_party/smaa/LICENSE.txt','third_party/fxaa/CMAA2-license.txt','third_party/XPLM-SDK-license.txt','assets/branding/aa-icon.ico','build/live-layer/native-aa-live.dll','build/live-layer/build-receipt.json','build/live-layer/test-receipt.json','build/live-layer/activation-receipt.json','build/plugin/NativeAA/64/win.xpl','build/plugin/build-receipt.json')
foreach($inputFile in @($build.inputs)+@($ui.inputs)+@($setup.inputs)){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $inputFile.path) -Algorithm SHA256).Hash -ne $inputFile.sha256){throw ('Build input changed: '+$inputFile.path)}
 $paths+=$inputFile.path
}
$paths=@($paths|Sort-Object -Unique)
$paths+='docs/RENDERER-SELECTION.md'
$repo=[IO.Path]::GetFullPath($PSScriptRoot)+[IO.Path]::DirectorySeparatorChar
foreach($relative in $paths){
 $source=if($relative -eq 'NativeAA-Setup.exe'){Join-Path $PSScriptRoot 'build/setup/NativeAA-Setup.exe'}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $relative))}
 if(-not $source.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Package path is outside source or redirected'}
 $target=Join-Path $stage $relative
 New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force|Out-Null
 Copy-Item -LiteralPath $source -Destination $target
}
$sourceCommit=(& git -C $PSScriptRoot rev-parse HEAD)
if($LASTEXITCODE -ne 0){throw 'Unable to record source commit'}
$manifest=[ordered]@{product='Make XP Anti-Aliasing Great Again';version='0.3.0-alpha';platform='Windows-x64';sourceCommit=$sourceCommit;rendererSHA256=$dll.Hash;pluginSHA256=$plugin.Hash;files=@($paths|ForEach-Object{[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $stage $_) -Algorithm SHA256).Hash}})}
[IO.File]::WriteAllText((Join-Path $stage 'RELEASE-MANIFEST.json'),($manifest|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal
$zipHash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'),($zipHash+'  '+$name+".zip`n"),[Text.UTF8Encoding]::new($false))
Write-Output ('Packaged '+$paths.Count+' inventoried payload files: '+$zip)
Write-Output ('SHA256 '+$zipHash)
