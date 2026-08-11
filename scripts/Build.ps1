$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$setup = Join-Path $PSScriptRoot 'Setup-Dependencies.ps1'
$solution = Join-Path $repoRoot 'NorthLitAW2.sln'

& $setup

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio 2022 with Desktop development with C++.'
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild was not found. Install the Visual Studio 2022 C++ workload.'
}

Write-Host "[NorthLit] MSBuild: $msbuild"
& $msbuild $solution /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "NorthLit build failed with exit code $LASTEXITCODE."
}

Write-Host '[NorthLit] Release x64 build completed.'
