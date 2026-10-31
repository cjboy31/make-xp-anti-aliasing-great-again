param([Parameter(Mandatory)][string]$Simulator,[switch]$NoOrdinaryLauncherIntegration)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Simulator).Path
if(-not(Test-Path -LiteralPath (Join-Path $root 'X-Plane.exe') -PathType Leaf) -or -not(Test-Path -LiteralPath (Join-Path $root 'Resources/plugins') -PathType Container)){throw 'Select the X-Plane installation root'}
if(Get-Process -Name X-Plane -ErrorAction SilentlyContinue){throw 'Close X-Plane normally before updating Native AA'}
$layer=Join-Path $PSScriptRoot 'build/live-layer'
$plugin=Join-Path $PSScriptRoot 'build/plugin/NativeAA/64/win.xpl'
$build=Get-Content -LiteralPath (Join-Path $layer 'build-receipt.json') -Raw|ConvertFrom-Json
$test=Get-Content -LiteralPath (Join-Path $layer 'test-receipt.json') -Raw|ConvertFrom-Json
$ui=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/plugin/build-receipt.json') -Raw|ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath (Join-Path $layer 'native-aa-live.dll') -Algorithm SHA256).Hash
if($build.status -ne 'PASS' -or $test.status -ne 'PASS' -or $ui.status -ne 'PASS' -or $hash -ne $build.dllSHA256 -or $hash -ne $test.dllSHA256 -or (Get-FileHash -LiteralPath $plugin -Algorithm SHA256).Hash -ne $ui.pluginSHA256){throw 'Current DLL/plugin require matching successful build and test receipts'}
if(-not $NoOrdinaryLauncherIntegration){
 $activation=Get-Content -LiteralPath (Join-Path $layer 'activation-receipt.json') -Raw|ConvertFrom-Json
 if($activation.status -ne 'PASS' -or $activation.dllSHA256 -ne $hash -or $activation.manifestSourceSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'tools/LayerManifest.ps1') -Algorithm SHA256).Hash){throw 'Ordinary-launch integration requires current DLL and manifest activation-isolation checks'}
}
foreach($inputFile in @($build.inputs)+@($ui.inputs)){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $inputFile.path) -Algorithm SHA256).Hash -ne $inputFile.sha256){throw ('Source/shader changed after checked build: '+$inputFile.path)}
}
 $shaderFiles=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'build/vulkan/shaders') -File|Where-Object{$_.Name -match '^(live-smaa-[0-3]-[0-2]|fxaa-[02]|native-taa|native-depth-(1|ms))\.comp\.spv$'})
 if($shaderFiles.Count -ne 17){throw 'Expected 12 SMAA-quality, 2 FXAA and 3 native TAA/depth shaders'}
$destination=[IO.Path]::GetFullPath((Join-Path $root 'Resources/plugins/NativeAA'))
$expected=[IO.Path]::GetFullPath((Join-Path $root 'Resources/plugins'))+[IO.Path]::DirectorySeparatorChar
if(-not $destination.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $destination -Leaf) -ne 'NativeAA'){throw 'Destination escaped the selected plugin directory'}
foreach($relative in @('','64','renderer','renderer/shaders','notices')){
 $directory=if($relative){Join-Path $destination $relative}else{$destination}
 if((Test-Path -LiteralPath $directory) -and ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw ('Refusing redirected plugin directory: '+$directory)}
}
$previousFile=Join-Path $destination 'installation.json'
$previous=if(Test-Path -LiteralPath $previousFile){Get-Content -LiteralPath $previousFile -Raw|ConvertFrom-Json}else{$null}
$installedDllName='native-aa-'+$hash.Substring(0,12).ToLowerInvariant()+'.dll'
$owned=@('64/win.xpl',('renderer/'+$installedDllName),'renderer/native-aa.json','README.txt','notices/NativeAA-MIT.txt','notices/SMAA-MIT.txt','notices/FXAA-source-and-BSD-notice.txt','notices/FXAA-Intel-Apache.txt','notices/XPLM-SDK-license.txt')
$owned+=@($shaderFiles|ForEach-Object{'renderer/shaders/'+$_.Name})
if(-not $NoOrdinaryLauncherIntegration -or (Test-Path -LiteralPath (Join-Path $destination 'standalone.enabled'))){$owned+='standalone.enabled'}
# Check all existing leaf/ancestor paths before copying or retiring owned files.
$priorPaths=if($previous){@($previous.files|ForEach-Object{$_.path})}else{@()}
foreach($relative in @($owned)+@($priorPaths)+@('installation.json')){
 $absolute=[IO.Path]::GetFullPath((Join-Path $destination $relative))
 if([IO.Path]::IsPathRooted($relative) -or $relative.Contains(':') -or -not $absolute.StartsWith($destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Installation inventory path escaped NativeAA'}
 $check=$absolute
 while($check -and ($check -eq $destination -or $check.StartsWith($destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))){
  if((Test-Path -LiteralPath $check) -and ((Get-Item -LiteralPath $check -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Refusing redirected installation file/directory'}
  $check=Split-Path -Parent $check
 }
}
foreach($folder in @('64','renderer/shaders','notices')){New-Item -ItemType Directory -Path (Join-Path $destination $folder) -Force|Out-Null}
Copy-Item -LiteralPath $plugin -Destination (Join-Path $destination '64/win.xpl') -Force
if(-not(Test-Path -LiteralPath (Join-Path $destination ('renderer/'+$installedDllName)))){
 Copy-Item -LiteralPath (Join-Path $layer 'native-aa-live.dll') -Destination (Join-Path $destination ('renderer/'+$installedDllName))
}elseif((Get-FileHash -LiteralPath (Join-Path $destination ('renderer/'+$installedDllName)) -Algorithm SHA256).Hash -ne $hash){throw 'Installed renderer filename/hash mismatch'}
foreach($file in $shaderFiles){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination ('renderer/shaders/'+$file.Name)) -Force}
foreach($pair in @(@('LICENSE','notices/NativeAA-MIT.txt'),@('third_party/smaa/LICENSE.txt','notices/SMAA-MIT.txt'),@('third_party/fxaa/Fxaa3_11.h','notices/FXAA-source-and-BSD-notice.txt'),@('third_party/fxaa/CMAA2-license.txt','notices/FXAA-Intel-Apache.txt'),@('third_party/XPLM-SDK-license.txt','notices/XPLM-SDK-license.txt'),@('docs/CONTROLS.md','README.txt'))){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $pair[0]) -Destination (Join-Path $destination $pair[1]) -Force
}
$manifest=Join-Path $destination 'renderer/native-aa.json'
. (Join-Path $PSScriptRoot 'tools/LayerManifest.ps1')
$body=New-NativeAALayerManifest $installedDllName
[IO.File]::WriteAllText($manifest,($body|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
if(-not $NoOrdinaryLauncherIntegration){
 [IO.File]::WriteAllText((Join-Path $destination 'standalone.enabled'),"NativeAAStandalone=1`n",[Text.UTF8Encoding]::new($false))
 $key='HKCU:\Software\Khronos\Vulkan\ImplicitLayers'
 # Never recreate an existing registry key: other layers own values here too.
 if(-not(Test-Path -LiteralPath $key)){New-Item -Path $key|Out-Null}
 New-ItemProperty -Path $key -Name $manifest -PropertyType DWord -Value 0 -Force|Out-Null
}
$ownedSet=@{};foreach($relative in $owned){$ownedSet[$relative.Replace('\','/').ToLowerInvariant()]=$true}
if($previous -and [string]::Equals($previous.simulator,$root,[StringComparison]::OrdinalIgnoreCase)){
 foreach($oldFile in $previous.files){
  $relative=([string]$oldFile.path).Replace('\','/')
  if($relative -eq 'installation.json' -or $ownedSet.ContainsKey($relative.ToLowerInvariant())){continue}
  $absolute=[IO.Path]::GetFullPath((Join-Path $destination $relative))
  if(-not $absolute.StartsWith($destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Previous inventory path escaped NativeAA'}
  if((Test-Path -LiteralPath $absolute -PathType Leaf) -and (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash -eq $oldFile.sha256){
   try{Remove-Item -LiteralPath $absolute -ErrorAction Stop}catch{Write-Output ('Previous owned file is still in use; retry cleanup after its application exits: '+$relative)}
  }
 }
}
# Record only shipped files, never claim ownership of a user's additional files.
$inventory=@($owned|ForEach-Object{[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $destination $_) -Algorithm SHA256).Hash}})
[IO.File]::WriteAllText((Join-Path $destination 'installation.json'),([ordered]@{product='NativeAA';version='0.3.2-alpha';simulator=$root;files=$inventory}|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output ('Installed Native AA 0.3.2-alpha controls and checked AA renderer: '+$destination)
Write-Output 'Use Plugins > Native AA > Open AA controls. Set X-Plane input scale to Full. No FSR/controller/camera settings were changed.'
