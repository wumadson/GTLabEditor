[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$StagingRoot = '',
    [string]$OutputRoot = '',
    [string]$InnoRoot = $env:INNO_SETUP_ROOT
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
if (-not $StagingRoot) { $StagingRoot = Join-Path $RepoRoot 'dist\staging\windows\GTLabEditor' }
if (-not $OutputRoot) { $OutputRoot = Join-Path $RepoRoot 'dist\windows' }
$StagingRoot = [IO.Path]::GetFullPath($StagingRoot)
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

if (-not (Test-Path -LiteralPath (Join-Path $StagingRoot 'GTLabEditor.exe'))) {
    throw "Staged executable not found below: $StagingRoot"
}

$candidates = @()
if ($InnoRoot) { $candidates += (Join-Path $InnoRoot 'ISCC.exe') }
$candidates += @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$iscc = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
if (-not $iscc) { throw 'ISCC.exe 6.7.3 was not found. Pass -InnoRoot or set INNO_SETUP_ROOT.' }

$versionText = ((& $iscc /? 2>&1) | Out-String)
if ($versionText -notmatch 'Inno Setup 6') { throw "Unexpected ISCC version:`n$versionText" }

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$script = Join-Path $RepoRoot 'installer\windows\GTLabEditor.iss'
$compilerOutput = @(& $iscc "/DStagingDir=$StagingRoot" "/DOutputDir=$OutputRoot" $script 2>&1)
$compilerOutput | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE." }
if (($compilerOutput -join "`n") -notmatch 'Compiler engine version:\s+Inno Setup 6\.7\.3') {
    throw "Expected the pinned Inno Setup 6.7.3 compiler."
}

$setup = Join-Path $OutputRoot 'GTLabEditor-1.0.0-Windows-x64-Setup.exe'
if (-not (Test-Path -LiteralPath $setup)) { throw "Expected installer not found: $setup" }
$hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "INSTALLER PASS: $setup"
Write-Host "SETUP SHA256: $hash"
Write-Output $setup
