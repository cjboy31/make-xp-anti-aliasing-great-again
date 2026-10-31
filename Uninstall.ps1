param([string]$Simulator,[switch]$NoOrdinaryLauncherIntegration)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'tools/Select-Simulator.ps1')
$selected=Select-NativeAASimulator $Simulator
if(-not $selected){Write-Output 'Uninstall cancelled. No changes made.';return}
$root=(Resolve-Path -LiteralPath $selected).Path
if(-not(Test-Path -LiteralPath (Join-Path $root 'X-Plane.exe') -PathType Leaf)){throw 'Select the X-Plane installation root'}
if(Get-Process -Name X-Plane -ErrorAction SilentlyContinue){throw 'Close X-Plane normally before uninstalling Native AA'}
$destination=[IO.Path]::GetFullPath((Join-Path $root 'Resources/plugins/NativeAA'))
$prefix=$destination+[IO.Path]::DirectorySeparatorChar
$inventoryPath=Join-Path $destination 'installation.json'
foreach($path in @($destination,$inventoryPath)){
 if((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Refusing redirected installation/inventory'}
}
if(-not(Test-Path -LiteralPath $inventoryPath -PathType Leaf)){throw 'No Native AA installation inventory found. No files removed.'}
$inventory=Get-Content -LiteralPath $inventoryPath -Raw|ConvertFrom-Json
if($inventory.product -ne 'NativeAA' -or -not [string]::Equals($inventory.simulator,$root,[StringComparison]::OrdinalIgnoreCase)){throw 'Installation inventory does not match this simulator'}
# Validate every candidate and every containing directory before any mutation.
$candidates=@(foreach($file in $inventory.files){
 $relative=[string]$file.path
 if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)' -or $relative.Contains(':') -or $file.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'Invalid installation inventory path/hash'}
 $absolute=[IO.Path]::GetFullPath((Join-Path $destination $relative))
 if(-not $absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Inventory path escaped NativeAA'}
 $check=$absolute
 while($check -and ($check -eq $destination -or $check.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase))){
  if((Test-Path -LiteralPath $check) -and ((Get-Item -LiteralPath $check -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Refusing redirected installation path'}
  $check=Split-Path -Parent $check
 }
 [pscustomobject]@{path=$absolute;sha256=$file.sha256}
})
if(-not $NoOrdinaryLauncherIntegration){
 $key='HKCU:\Software\Khronos\Vulkan\ImplicitLayers'
 $manifest=Join-Path $destination 'renderer/native-aa.json'
 if(Test-Path -LiteralPath $key){
  $registry=Get-Item -LiteralPath $key
  if($registry.GetValueNames() -contains $manifest){Remove-ItemProperty -LiteralPath $key -Name $manifest}
 }
}
$preserved=@()
foreach($file in $candidates){
 if(Test-Path -LiteralPath $file.path -PathType Leaf){
  if((Get-FileHash -LiteralPath $file.path -Algorithm SHA256).Hash -eq $file.sha256){Remove-Item -LiteralPath $file.path}
  else {$preserved+=$file.path}
 }
}
Remove-Item -LiteralPath $inventoryPath
foreach($relative in @('renderer/shaders','renderer','notices','64','')){
 $folder=if($relative){Join-Path $destination $relative}else{$destination}
 if((Test-Path -LiteralPath $folder -PathType Container) -and -not(Get-ChildItem -LiteralPath $folder -Force)){Remove-Item -LiteralPath $folder}
}
Write-Output 'Native AA uninstalled. Simulator graphics, controllers, other plugins and saved NativeAA.ini preferences are retained.'
foreach($file in $preserved){Write-Output ('Preserved user-modified file: '+$file)}
