param(
    [Parameter(Mandatory)][string]$Compiler,
    [Parameter(Mandatory)][string]$VulkanHeaders,
    [Parameter(Mandatory)][string]$Glslang
)
$ErrorActionPreference = 'Stop'
$cc = (Resolve-Path -LiteralPath $Compiler).Path
$headers = (Resolve-Path -LiteralPath $VulkanHeaders).Path
$glsl = (Resolve-Path -LiteralPath $Glslang).Path
$compilerBin = Split-Path $cc -Parent
$out = Join-Path $PSScriptRoot 'build/vulkan'
$shaders = Join-Path $out 'shaders'
New-Item -ItemType Directory -Path $shaders -Force | Out-Null
foreach ($aaPass in 0..2) {
    foreach ($aaStage in @('vert', 'frag', 'comp')) {
        & $glsl -V --target-env vulkan1.1 ('-DAA_PASS=' + $aaPass) ('-I' + (Join-Path $PSScriptRoot 'third_party/smaa')) (Join-Path $PSScriptRoot ('shaders/smaa.' + $aaStage)) -o (Join-Path $shaders ('smaa-' + $aaPass + '.' + $aaStage + '.spv'))
        if ($LASTEXITCODE -ne 0) { throw 'SMAA shader compilation failed' }
    }
}
& $glsl -V --target-env vulkan1.1 (Join-Path $PSScriptRoot 'shaders/camera_motion.comp') -o (Join-Path $shaders 'camera-motion.comp.spv')
if ($LASTEXITCODE -ne 0) { throw 'Camera motion shader compilation failed' }
$exports = & (Join-Path $compilerBin 'llvm-readobj.exe') --coff-exports (Join-Path $env:WINDIR 'System32/vulkan-1.dll')
$names = @($exports | ForEach-Object { if ($_ -match '^\s*Name: (vk\w+)$') { $matches[1] } })
if ($names.Count -lt 100) { throw 'Installed Vulkan loader exports unavailable' }
$definition = Join-Path $out 'vulkan-loader.def'
$library = Join-Path $out 'vulkan-loader.lib'
[IO.File]::WriteAllLines($definition, (@('LIBRARY vulkan-1.dll', 'EXPORTS') + $names), [Text.UTF8Encoding]::new($false))
& (Join-Path $compilerBin 'llvm-dlltool.exe') -m i386:x86-64 -d $definition -l $library
if ($LASTEXITCODE -ne 0) { throw 'Vulkan import-library creation failed' }
$flags = @('-std=c++17', '-O2', '-static', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-missing-field-initializers', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', ('-I' + (Join-Path $headers 'include')), ('-I' + (Join-Path $PSScriptRoot 'include')), ('-I' + (Join-Path $PSScriptRoot 'third_party/smaa')))
& $cc @flags (Join-Path $PSScriptRoot 'src/vulkan/smaa_pipeline.cpp') (Join-Path $PSScriptRoot 'src/vulkan/compute_smaa_pipeline.cpp') (Join-Path $PSScriptRoot 'src/vulkan/camera_motion_pipeline.cpp') (Join-Path $PSScriptRoot 'src/motion.cpp') (Join-Path $PSScriptRoot 'tests/smaa_vulkan_tests.cpp') $library -o (Join-Path $out 'smaa_vulkan_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Vulkan fixture compilation failed' }
& $cc @flags (Join-Path $PSScriptRoot 'src/motion.cpp') (Join-Path $PSScriptRoot 'tests/motion_tests.cpp') -o (Join-Path $out 'motion_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Motion contract compilation failed' }
Write-Output 'Built offscreen SMAA checks. No simulator or activation setting was changed.'
