$ErrorActionPreference='Stop'
# Filesystem-only fixture. Never registers a real layer or starts the simulator.
$root=Join-Path $PSScriptRoot ('build/install-check-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path (Join-Path $root 'Resources/plugins/NativeAA') -Force|Out-Null
[IO.File]::WriteAllText((Join-Path $root 'X-Plane.exe'),'fixture placeholder - never executed')
$plugin=Join-Path $root 'Resources/plugins/NativeAA'
$note=Join-Path $plugin 'user-note.txt'
[IO.File]::WriteAllText($note,'Keep this user file')
& (Join-Path $PSScriptRoot 'Install-DevelopmentBuild.ps1') -Simulator $root -NoOrdinaryLauncherIntegration
$inventoryPath=Join-Path $plugin 'installation.json'
$inventory=Get-Content -LiteralPath $inventoryPath -Raw|ConvertFrom-Json
if($inventory.files.path -contains 'user-note.txt'){throw 'Installer claimed ownership of an unknown user file'}
$obsolete=Join-Path $plugin 'obsolete-owned.dll'
$changed=Join-Path $plugin 'changed-owned.txt'
[IO.File]::WriteAllText($obsolete,'old owned version')
[IO.File]::WriteAllText($changed,'old owned text')
$inventory.files+=@([pscustomobject]@{path='obsolete-owned.dll';sha256=(Get-FileHash -LiteralPath $obsolete -Algorithm SHA256).Hash},[pscustomobject]@{path='changed-owned.txt';sha256=(Get-FileHash -LiteralPath $changed -Algorithm SHA256).Hash})
[IO.File]::WriteAllText($inventoryPath,($inventory|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($changed,'user edited this file')
& (Join-Path $PSScriptRoot 'Install-DevelopmentBuild.ps1') -Simulator $root -NoOrdinaryLauncherIntegration
if(Test-Path -LiteralPath $obsolete){throw 'Obsolete unmodified owned file was retained'}
if((Get-Content -LiteralPath $note -Raw) -ne 'Keep this user file' -or (Get-Content -LiteralPath $changed -Raw) -ne 'user edited this file'){throw 'User file was altered'}
$manifest=Get-Content -LiteralPath (Join-Path $plugin 'renderer/native-aa.json') -Raw|ConvertFrom-Json
if($manifest.layer.library_path -notmatch '^\.\\native-aa-[a-f0-9]{12}\.dll$'){throw 'Installer did not use the checked Windows manifest path'}
if(Test-Path -LiteralPath (Join-Path $plugin 'standalone.enabled')){throw 'No-integration install unexpectedly enabled startup'}
Write-Output 'Development installer PASS: initial/update, owned-file cleanup, unknown/modified user-file preservation and shared Windows manifest.'
