$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$external = Join-Path $repoRoot 'NorthLitAW2\external'
$minHookDir = Join-Path $external 'minhook'
$minHookCommit = 'd94c64d32ea37bc4f5ee47d580709f70c6fb6080'
$archiveUrl = "https://github.com/TsudaKageyu/minhook/archive/$minHookCommit.zip"
$tempZip = Join-Path $env:TEMP "minhook-$minHookCommit.zip"
$tempExtract = Join-Path $env:TEMP "minhook-$minHookCommit"

New-Item -ItemType Directory -Force -Path $external | Out-Null

$requiredHeader = Join-Path $minHookDir 'include\MinHook.h'
if (Test-Path $requiredHeader) {
    Write-Host '[NorthLit] MinHook already present.'
    Write-Host '[NorthLit] Dependencies ready.'
    return
}

if (Test-Path $minHookDir) {
    Remove-Item -Recurse -Force $minHookDir
}
if (Test-Path $tempExtract) {
    Remove-Item -Recurse -Force $tempExtract
}
if (Test-Path $tempZip) {
    Remove-Item -Force $tempZip
}

Write-Host "[NorthLit] Downloading MinHook $minHookCommit..."
Invoke-WebRequest -UseBasicParsing -Uri $archiveUrl -OutFile $tempZip

Write-Host '[NorthLit] Extracting MinHook...'
Expand-Archive -Path $tempZip -DestinationPath $tempExtract -Force

$extractedDir = Get-ChildItem -Path $tempExtract -Directory | Select-Object -First 1
if (-not $extractedDir) {
    throw 'MinHook archive extraction failed.'
}

Move-Item -Path $extractedDir.FullName -Destination $minHookDir

if (-not (Test-Path $requiredHeader)) {
    throw 'MinHook setup failed: include\MinHook.h was not found after extraction.'
}

Remove-Item -Force $tempZip -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $tempExtract -ErrorAction SilentlyContinue

Write-Host '[NorthLit] Dependencies ready. Git is not required.'
Write-Host '[NorthLit] Open NorthLitAW2.sln and build Release | x64.'
