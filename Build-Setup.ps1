$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'build/setup'
New-Item -ItemType Directory -Path $out -Force|Out-Null
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /warnaserror+ /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll (('/win32icon:')+(Join-Path $PSScriptRoot 'assets/branding/aa-icon.ico')) (('/out:')+(Join-Path $out 'NativeAA-Setup.exe')) (Join-Path $PSScriptRoot 'tools/NativeAASetup.cs')
if($LASTEXITCODE -ne 0){throw 'Setup build failed'}
$inputs=@('tools/NativeAASetup.cs','assets/branding/aa-icon.ico')|ForEach-Object{[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $_) -Algorithm SHA256).Hash}}
[IO.File]::WriteAllText((Join-Path $out 'build-receipt.json'),([ordered]@{status='PASS';setupSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'NativeAA-Setup.exe') -Algorithm SHA256).Hash;inputs=@($inputs)}|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
