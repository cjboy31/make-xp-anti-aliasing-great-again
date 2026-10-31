param([Parameter(Mandatory)][string]$Zip,[switch]$WithLauncherRegistration)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Zip).Path)
try{
 $entries=@($archive.Entries|Where-Object{$_.Name})
 foreach($entry in $entries){
  if($entry.FullName -match '(^|[\\/])\.\.([\\/]|$)|^[\\/]|:' -or $entry.FullName -match '(?i)\.(pdb|rdc|mp4|jpg|png|onnx|safetensors)$|nvngx|fsr2ffx|amd_fidelityfx' -or ($entry.Name -match '(?i)\.exe$' -and $entry.Name -ne 'NativeAA-Setup.exe')){throw 'Unexpected or unsafe ZIP payload'}
 }
 if(@($entries|Where-Object{$_.Name -match '(?i)\.(dll|xpl)$'}).Count -ne 2){throw 'Only our renderer DLL and plugin XPL may be included'}
 if(@($entries|Where-Object{$_.Name -match '(?i)\.spv$'}).Count -ne 20){throw 'Expected 17 production shaders plus three receipt-checked offscreen shaders'}
}finally{$archive.Dispose()}
$temporary=Join-Path $PSScriptRoot ('build/package check '+[guid]::NewGuid().ToString('N').Substring(0,8))
Expand-Archive -LiteralPath $Zip -DestinationPath $temporary
$package=Join-Path $temporary 'Make-XP-AA-0.3.1-alpha-Windows-x64'
$manifest=Get-Content -LiteralPath (Join-Path $package 'RELEASE-MANIFEST.json') -Raw|ConvertFrom-Json
if($manifest.version -ne '0.3.1-alpha' -or $manifest.platform -ne 'Windows-x64'){throw 'Incorrect release version/platform'}
if(@(Get-ChildItem -LiteralPath $package -Recurse -File).Count -ne @($manifest.files).Count+1){throw 'Uninventoried package files'}
foreach($file in $manifest.files){
 if((Get-FileHash -LiteralPath (Join-Path $package $file.path) -Algorithm SHA256).Hash -ne $file.sha256){throw ('Package hash mismatch: '+$file.path)}
}
foreach($script in @(Get-ChildItem -LiteralPath $package -Filter *.ps1 -Recurse -File)){
 $tokens=$null;$errors=$null
 [System.Management.Automation.Language.Parser]::ParseFile($script.FullName,[ref]$tokens,[ref]$errors)|Out-Null
 if($errors){throw ('Package script syntax error: '+$script.Name)}
}
$simulator=Join-Path $temporary 'Selected Simulator'
New-Item -ItemType Directory -Path (Join-Path $simulator 'Resources/plugins') -Force|Out-Null
[IO.File]::WriteAllText((Join-Path $simulator 'X-Plane.exe'),'fixture only; never executed')
$key='HKCU:\Software\Khronos\Vulkan\ImplicitLayers'
$before=@{}
if($WithLauncherRegistration -and (Test-Path -LiteralPath $key)){
 $registry=Get-Item -LiteralPath $key
 foreach($name in $registry.GetValueNames()){$before[$name]=$registry.GetValue($name)}
}
function Invoke-Setup([string]$Action,[int]$Expected=0){
 $arguments=@($Action,('"'+$simulator+'"'))
 if(-not $WithLauncherRegistration){$arguments+='--no-registration'}
 $setupRun=Start-Process -FilePath (Join-Path $package 'NativeAA-Setup.exe') -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
 if($setupRun.ExitCode -ne $Expected){throw ('Setup '+$Action+' exit '+$setupRun.ExitCode+'; expected '+$Expected)}
}
Invoke-Setup '--install'
$plugin=Join-Path $simulator 'Resources/plugins/NativeAA'
$installed=Get-Content -LiteralPath (Join-Path $plugin 'installation.json') -Raw|ConvertFrom-Json
if($installed.version -ne $manifest.version){throw 'Installed version does not match release'}
if((Get-FileHash -LiteralPath (Join-Path $plugin '64/win.xpl') -Algorithm SHA256).Hash -ne $manifest.pluginSHA256){throw 'Installed plugin differs from tested plugin'}
$manifestPath=Join-Path $plugin 'renderer/native-aa.json'
if($WithLauncherRegistration){
 if((Get-Content -LiteralPath (Join-Path $plugin 'standalone.enabled') -Raw).Trim() -ne 'NativeAAStandalone=1' -or (Get-Item -LiteralPath $key).GetValue($manifestPath,-1) -ne 0){throw 'Ordinary-launch registration not enabled'}
}
$inventoryPath=Join-Path $plugin 'installation.json'
$validInventory=Get-Content -LiteralPath $inventoryPath -Raw
$malformed=$validInventory|ConvertFrom-Json
$malformed.files+=@([pscustomobject]@{path='../outside.txt';sha256=('0'*64)})
[IO.File]::WriteAllText($inventoryPath,($malformed|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
Invoke-Setup '--install' 1
Invoke-Setup '--uninstall' 1
if((Get-FileHash -LiteralPath (Join-Path $plugin '64/win.xpl') -Algorithm SHA256).Hash -ne $manifest.pluginSHA256){throw 'Malformed inventory rejection changed installed plugin'}
[IO.File]::WriteAllText($inventoryPath,$validInventory,[Text.UTF8Encoding]::new($false))
# Upgrade retirement must preserve user-edited and unknown files.
$old=Join-Path $plugin 'old-owned.dll';$unknown=Join-Path $plugin 'user-note.txt'
[IO.File]::WriteAllText($old,'obsolete owned file')
[IO.File]::WriteAllText($unknown,'user data')
$installed.files+=@([pscustomobject]@{path='old-owned.dll';sha256=(Get-FileHash -LiteralPath $old -Algorithm SHA256).Hash})
[IO.File]::WriteAllText($inventoryPath,($installed|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
Invoke-Setup '--install'
if(Test-Path -LiteralPath $old){throw 'Upgrade retained obsolete owned file'}
$edited=Join-Path $plugin 'README.txt'
[IO.File]::WriteAllText($edited,'edited user notes')
Invoke-Setup '--uninstall'
if((Get-Content -LiteralPath $unknown -Raw) -ne 'user data' -or (Get-Content -LiteralPath $edited -Raw) -ne 'edited user notes'){throw 'Setup altered unknown/edited files'}
Remove-Item -LiteralPath $unknown,$edited
Remove-Item -LiteralPath $plugin
if(Test-Path -LiteralPath $plugin){throw 'Clean package uninstall left owned files'}
if($WithLauncherRegistration){
 $registry=Get-Item -LiteralPath $key
 if($registry.GetValueNames() -contains $manifestPath){throw 'Uninstall retained its registration'}
 if($registry.GetValueNames().Count -ne $before.Count){throw 'Another layer registration was added/removed'}
 foreach($name in $before.Keys){if($registry.GetValue($name) -ne $before[$name]){throw 'Another layer registration was modified'}}
}
Write-Output ('Release ZIP PASS: '+$manifest.files.Count+' hashes, Windows script parsing, exact tested binaries, selected-folder install/uninstall, launcher registration='+[bool]$WithLauncherRegistration)
