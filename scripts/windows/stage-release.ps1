[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$BuildRoot = '',
    [string]$StagingRoot = '',
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$CrtRoot = '',
    [string]$ManifestPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE." }
}

function Assert-SafeChildPath([string]$Path, [string]$Parent, [string]$Label) {
    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullParent = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($fullParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain below $Parent. Resolved path: $fullPath"
    }
}

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
if (-not $BuildRoot) { $BuildRoot = Join-Path $RepoRoot 'build\windows-release' }
if (-not $StagingRoot) { $StagingRoot = Join-Path $RepoRoot 'dist\staging\windows\GTLabEditor' }
if (-not $ManifestPath) { $ManifestPath = Join-Path $RepoRoot 'dist\windows\build-manifest.txt' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$StagingRoot = [IO.Path]::GetFullPath($StagingRoot)
$ManifestPath = [IO.Path]::GetFullPath($ManifestPath)
Assert-SafeChildPath $StagingRoot (Join-Path $RepoRoot 'dist\staging') 'StagingRoot'

if (-not $QtRoot) {
    $localQt = 'C:\Qt\5.15.2\msvc2019_64'
    if (Test-Path -LiteralPath $localQt) { $QtRoot = $localQt }
}
if (-not $QtRoot) { throw 'QtRoot is required. Pass -QtRoot or set QT_ROOT_DIR.' }
$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
if (-not (Test-Path -LiteralPath $windeployqt)) { throw "windeployqt.exe not found: $windeployqt" }

$sourceExe = Join-Path $BuildRoot 'packager\GTLabEditor.exe'
if (-not (Test-Path -LiteralPath $sourceExe)) { throw "Release executable not found: $sourceExe" }

if (Test-Path -LiteralPath $StagingRoot) { Remove-Item -LiteralPath $StagingRoot -Recurse -Force }
New-Item -ItemType Directory -Path $StagingRoot | Out-Null
Copy-Item -LiteralPath $sourceExe -Destination (Join-Path $StagingRoot 'GTLabEditor.exe')

& $windeployqt --release --compiler-runtime --no-translations --dir $StagingRoot (Join-Path $StagingRoot 'GTLabEditor.exe')
Assert-LastExitCode 'windeployqt'

# windeployqt may deploy the redistributable installer instead of the app-local
# CRT DLLs. Copy the complete VC142 CRT assembly so the portable staging tree
# does not depend on a machine-wide Visual C++ installation.
if (-not $CrtRoot -and $env:VCToolsRedistDir) {
    $CrtRoot = Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC142.CRT'
}
if (-not $CrtRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsRoot = (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
        $redistVersion = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Redist\MSVC') -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like '14.29.*' } | Sort-Object Name -Descending | Select-Object -First 1
        if ($redistVersion) { $CrtRoot = Join-Path $redistVersion.FullName 'x64\Microsoft.VC142.CRT' }
    }
}
if (-not $CrtRoot -or -not (Test-Path -LiteralPath $CrtRoot)) {
    throw "VC142 app-local CRT directory not found. Pass -CrtRoot or initialize the v142 build environment."
}
$CrtRoot = [IO.Path]::GetFullPath($CrtRoot)
$crtDlls = Get-ChildItem -LiteralPath $CrtRoot -Filter '*.dll' -File
if (-not $crtDlls) { throw "No app-local CRT DLLs found: $CrtRoot" }
$crtDlls | Copy-Item -Destination $StagingRoot
$redistributable = Join-Path $StagingRoot 'vc_redist.x64.exe'
if (Test-Path -LiteralPath $redistributable) { Remove-Item -LiteralPath $redistributable -Force }

$documents = [ordered]@{
    'LICENSE.GPL-2.0' = 'LICENSE.GPL-2.0'
    'license.txt' = 'license.txt'
    'GTLabEditor-NOTICE.txt' = 'GTLabEditor-NOTICE.txt'
    'installer\THIRD-PARTY-NOTICES.txt' = 'THIRD-PARTY-NOTICES.txt'
}
foreach ($source in $documents.Keys) {
    $sourcePath = Join-Path $RepoRoot $source
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Required notice not found: $sourcePath" }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $StagingRoot $documents[$source])
}

$required = @(
    'GTLabEditor.exe', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll',
    'Qt5Xml.dll', 'Qt5PrintSupport.dll', 'platforms\qwindows.dll',
    'LICENSE.GPL-2.0', 'GTLabEditor-NOTICE.txt', 'THIRD-PARTY-NOTICES.txt',
    'MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $StagingRoot $relative))) {
        throw "Required staging file missing: $relative"
    }
}
if (Get-ChildItem -LiteralPath $StagingRoot -Recurse -File | Where-Object { $_.Name -match '(?i)^preferences.*\.xml$|RDWM|RDID|Roland|winusb-lab' }) {
    throw 'Staging contains a personal preference or prohibited driver/LAB payload.'
}

$manifestDirectory = Split-Path -Parent $ManifestPath
New-Item -ItemType Directory -Force -Path $manifestDirectory | Out-Null
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('GT Lab Editor Windows staging manifest')
$buildInfoPath = Join-Path $BuildRoot 'build-info.json'
if (Test-Path -LiteralPath $buildInfoPath) {
    $info = Get-Content -LiteralPath $buildInfoPath -Raw | ConvertFrom-Json
    $lines.Add("Version: $($info.version)")
    $lines.Add("Commit: $($info.commit)")
    $lines.Add("Ref: $($info.ref)")
    $lines.Add("Build timestamp UTC: $($info.timestampUtc)")
    $lines.Add("Qt: $($info.qt)")
    $lines.Add("qmake: $($info.qmake)")
    $lines.Add("MSVC toolset: $($info.msvcToolset)")
    $lines.Add("Compiler: $($info.compiler)")
    $lines.Add("Windows SDK: $($info.windowsSdk)")
}
$lines.Add("Manifest timestamp UTC: $([DateTime]::UtcNow.ToString('o'))")
$lines.Add('')
$lines.Add('SHA256  SIZE  RELATIVE_PATH')
Get-ChildItem -LiteralPath $StagingRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($StagingRoot.Length).TrimStart('\').Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $lines.Add("$hash  $($_.Length)  $relative")
}
$lines | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Host "STAGING PASS: $StagingRoot"
Write-Output $StagingRoot
