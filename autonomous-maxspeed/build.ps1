param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$CubeIdeRoot = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE'
)
$ErrorActionPreference = 'Stop'
$projectName = Split-Path -Leaf $PSScriptRoot
$launcher = Join-Path $CubeIdeRoot 'headless-build.bat'
if (!(Test-Path -LiteralPath $launcher)) {
    throw 'STM32CubeIDE not found. Supply -CubeIdeRoot with the installation directory.'
}
# Eclipse requires its workspace to be OUTSIDE the imported project directory.
$buildWorkspace = Join-Path $env:TEMP "stm32-$projectName-workspace"
& $launcher -data $buildWorkspace -import $PSScriptRoot -build "$projectName/$Configuration"
if ($LASTEXITCODE -ne 0) { throw "Build failed: exit $LASTEXITCODE" }
$elf = Join-Path $PSScriptRoot "$Configuration/$projectName.elf"
if (!(Test-Path -LiteralPath $elf)) { throw "Build did not produce $elf" }
$objcopy = Get-ChildItem -Path (Join-Path $CubeIdeRoot 'plugins/*/tools/bin/arm-none-eabi-objcopy.exe') |
    Select-Object -First 1
if ($null -eq $objcopy) { throw 'arm-none-eabi-objcopy.exe not found' }
& $objcopy.FullName -O ihex $elf (Join-Path $PSScriptRoot "$Configuration/$projectName.hex")
if ($LASTEXITCODE -ne 0) { throw 'HEX generation failed' }
& $objcopy.FullName -O binary $elf (Join-Path $PSScriptRoot "$Configuration/$projectName.bin")
if ($LASTEXITCODE -ne 0) { throw 'BIN generation failed' }
Write-Output "Built $Configuration ELF, HEX and BIN."
