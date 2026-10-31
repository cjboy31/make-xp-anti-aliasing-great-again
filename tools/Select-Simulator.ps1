function Select-NativeAASimulator {
 param([string]$Simulator)
 if($Simulator){return $Simulator}
 Add-Type -AssemblyName System.Windows.Forms
 $dialog=New-Object System.Windows.Forms.FolderBrowserDialog
 try {
  $dialog.Description='Select your X-Plane 12 folder (the folder containing X-Plane.exe). Close X-Plane before continuing.'
  $dialog.ShowNewFolderButton=$false
  if($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK){return $null}
  return $dialog.SelectedPath
 } finally {$dialog.Dispose()}
}
