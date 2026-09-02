[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$VsInstallPath = '',
    [string]$InnoRoot = $env:INNO_SETUP_ROOT
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
$buildRoot = Join-Path $RepoRoot 'build\windows-release'
$stagingRoot = Join-Path $RepoRoot 'dist\staging\windows\GTLabEditor'
$outputRoot = Join-Path $RepoRoot 'dist\windows'
$manifest = Join-Path $outputRoot 'build-manifest.txt'

$buildArguments = @{
    RepoRoot = $RepoRoot
    BuildRoot = $buildRoot
    QtRoot = $QtRoot
}
if ($VsInstallPath) { $buildArguments.VsInstallPath = $VsInstallPath }
& (Join-Path $PSScriptRoot 'build-release.ps1') @buildArguments | Out-Host

& (Join-Path $PSScriptRoot 'stage-release.ps1') -RepoRoot $RepoRoot -BuildRoot $buildRoot -StagingRoot $stagingRoot -QtRoot $QtRoot -ManifestPath $manifest | Out-Host

$installerArguments = @{
    RepoRoot = $RepoRoot
    StagingRoot = $stagingRoot
    OutputRoot = $outputRoot
}
if ($InnoRoot) { $installerArguments.InnoRoot = $InnoRoot }
& (Join-Path $PSScriptRoot 'build-installer.ps1') @installerArguments | Out-Host

$exe = Join-Path $stagingRoot 'GTLabEditor.exe'
$setup = Join-Path $outputRoot 'GTLabEditor-1.0.0-Windows-x64-Setup.exe'
$zip = Join-Path $outputRoot 'GTLabEditor-1.0.0-Windows-x64-Portable.zip'
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $zip -CompressionLevel Optimal

$hashFile = Join-Path $outputRoot 'SHA256SUMS.txt'
$artifacts = @($exe, $setup, $zip)
$hashLines = foreach ($artifact in $artifacts) {
    if (-not (Test-Path -LiteralPath $artifact)) { throw "Artifact missing: $artifact" }
    $hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($artifact))"
}
$hashLines | Set-Content -LiteralPath $hashFile -Encoding ASCII

Add-Content -LiteralPath $manifest -Encoding UTF8 -Value @(
    '',
    'Packaging:',
    'Inno Setup: 6.7.3',
    "Installer SHA256: $((Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant())",
    "Portable ZIP SHA256: $((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant())"
)

$info = Get-Content -LiteralPath (Join-Path $buildRoot 'build-info.json') -Raw | ConvertFrom-Json
Write-Host ''
Write-Host 'GT LAB EDITOR WINDOWS BUILD COMPLETE'
Write-Host "VERSION: $($info.version)"
Write-Host "COMMIT: $($info.commit)"
Write-Host "EXE PATH: $exe"
Write-Host "SETUP PATH: $setup"
Write-Host "STAGING ZIP: $zip"
Write-Host "EXE SHA256: $((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant())"
Write-Host "SETUP SHA256: $((Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant())"
Write-Host "HASHES: $hashFile"
Write-Host "MANIFEST: $manifest"
