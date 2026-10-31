param([string]$Simulator)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'tools/Select-Simulator.ps1')
$selected=Select-NativeAASimulator $Simulator
if(-not $selected){Write-Output 'Installation cancelled. No changes made.';return}
& (Join-Path $PSScriptRoot 'Install-DevelopmentBuild.ps1') -Simulator $selected
Write-Output 'Installation complete. Start X-Plane with your usual launcher, then open Plugins > Native AA > Open AA controls.'
