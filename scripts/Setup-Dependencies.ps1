$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$external = Join-Path $repoRoot 'NorthLitAW2\external'
$minHookDir = Join-Path $external 'minhook'
$minHookCommit = 'd94c64d32ea37bc4f5ee47d580709f70c6fb6080'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git is required to fetch MinHook. Install Git for Windows and run this script again.'
}

New-Item -ItemType Directory -Force -Path $external | Out-Null

if (-not (Test-Path (Join-Path $minHookDir '.git'))) {
    if (Test-Path $minHookDir) {
        Remove-Item -Recurse -Force $minHookDir
    }

    Write-Host '[NorthLit] Cloning MinHook...'
    git clone https://github.com/TsudaKageyu/minhook.git $minHookDir
    if ($LASTEXITCODE -ne 0) { throw 'Failed to clone MinHook.' }
}

Write-Host "[NorthLit] Pinning MinHook to $minHookCommit..."
git -C $minHookDir fetch --all --tags --prune
if ($LASTEXITCODE -ne 0) { throw 'Failed to update MinHook repository.' }

git -C $minHookDir checkout --force $minHookCommit
if ($LASTEXITCODE -ne 0) { throw 'Failed to checkout the pinned MinHook commit.' }

Write-Host '[NorthLit] Dependencies ready.'
Write-Host '[NorthLit] Open NorthLitAW2.sln and build Release | x64.'
