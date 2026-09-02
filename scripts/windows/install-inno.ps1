[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:RUNNER_TEMP 'inno-setup-6.7.3'),
    [string]$Version = '6.7.3',
    [string]$ExpectedSha256 = '9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

if (-not $env:RUNNER_TEMP) { throw 'install-inno.ps1 is intended for an ephemeral CI runner.' }
$installer = Join-Path $env:RUNNER_TEMP "innosetup-$Version.exe"
$urlVersion = $Version.Replace('.', '_')
$url = "https://github.com/jrsoftware/issrc/releases/download/is-$urlVersion/innosetup-$Version.exe"

Invoke-WebRequest -Uri $url -OutFile $installer
$actual = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $ExpectedSha256) {
    throw "Inno Setup SHA-256 mismatch. Expected $ExpectedSha256, got $actual."
}

$process = Start-Process `
    -FilePath $installer `
    -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/CURRENTUSER', "/DIR=$InstallRoot") `
    -Wait `
    -PassThru
if ($process.ExitCode -ne 0) {
    throw "Inno Setup installer failed with exit code $($process.ExitCode)."
}
$iscc = Join-Path $InstallRoot 'ISCC.exe'
if (-not (Test-Path -LiteralPath $iscc)) { throw "ISCC.exe not found after install: $iscc" }
Write-Host "INNO SETUP $Version READY: $iscc"
