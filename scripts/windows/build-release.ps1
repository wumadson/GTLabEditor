[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$BuildRoot = '',
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$VsInstallPath = '',
    [string]$ToolsetVersion = '14.29',
    [string]$ExpectedCompilerPrefix = '19.29.',
    [string]$ExpectedQtVersion = '5.15.2',
    [string]$ExpectedQmakeVersion = '3.1',
    [string]$ExpectedWindowsSdkPrefix = '10.0.26100.'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

function Assert-SafeChildPath([string]$Path, [string]$Parent, [string]$Label) {
    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullParent = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($fullParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain below $Parent. Resolved path: $fullPath"
    }
}

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
if (-not $BuildRoot) {
    $BuildRoot = Join-Path $RepoRoot 'build\windows-release'
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
Assert-SafeChildPath $BuildRoot $RepoRoot 'BuildRoot'

if (-not $QtRoot) {
    $localQt = 'C:\Qt\5.15.2\msvc2019_64'
    if (Test-Path -LiteralPath $localQt) { $QtRoot = $localQt }
}
if (-not $QtRoot) { throw 'QtRoot is required. Pass -QtRoot or set QT_ROOT_DIR.' }
$QtRoot = [IO.Path]::GetFullPath($QtRoot)
$qmake = Join-Path $QtRoot 'bin\qmake.exe'
if (-not (Test-Path -LiteralPath $qmake)) { throw "qmake.exe not found: $qmake" }

if (-not $VsInstallPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere.exe not found: $vswhere" }
    $VsInstallPath = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.ComponentGroup.VC.Tools.142.x86.x64 -property installationPath | Select-Object -First 1)
    if (-not $VsInstallPath) {
        # Newer Visual Studio installers may not expose the legacy component
        # group through vswhere even when vcvars can select the v142 toolset.
        $VsInstallPath = (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
    }
}
if (-not $VsInstallPath) { throw 'A Visual Studio installation containing the v142 x64 tools was not found.' }
$vcvars = Join-Path $VsInstallPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

$environmentScript = [IO.Path]::ChangeExtension([IO.Path]::GetTempFileName(), '.cmd')
try {
    @(
        '@echo off',
        "call `"$vcvars`" -vcvars_ver=$ToolsetVersion >nul",
        'if errorlevel 1 exit /b %errorlevel%',
        'set'
    ) | Set-Content -LiteralPath $environmentScript -Encoding ASCII
    $environmentLines = & cmd.exe /d /c $environmentScript
    Assert-LastExitCode 'vcvars64'
} finally {
    if (Test-Path -LiteralPath $environmentScript) { Remove-Item -LiteralPath $environmentScript -Force }
}
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        # Some hosts expose both Path and PATH. Keep the vcvars-expanded one
        # instead of allowing the stale inherited variant to overwrite it.
        if ($name -ieq 'Path') {
            if ($value -match '\\VC\\Tools\\MSVC\\14\.29\.') {
                [Environment]::SetEnvironmentVariable('Path', $value, 'Process')
            }
        } else {
            [Environment]::SetEnvironmentVariable($name, $value, 'Process')
        }
    }
}

$compilerText = ((& cl.exe 2>&1) | Out-String)
if ($compilerText -notmatch "\b$([regex]::Escape($ExpectedCompilerPrefix))[0-9.]*\b") {
    throw "Expected cl.exe $ExpectedCompilerPrefix*, got:`n$compilerText"
}
if (-not $env:WindowsSDKVersion -or -not $env:WindowsSDKVersion.StartsWith($ExpectedWindowsSdkPrefix)) {
    throw "Expected Windows SDK $ExpectedWindowsSdkPrefix*, got '$($env:WindowsSDKVersion)'."
}
$compilerMatch = [regex]::Match($compilerText, '\b19\.29\.([0-9]+)\b')
if (-not $compilerMatch.Success) { throw 'Could not derive QMAKE_MSC_FULL_VER from cl.exe output.' }
$qmakeMscFullVer = '1929' + $compilerMatch.Groups[1].Value

$qmakeText = ((& $qmake -v 2>&1) | Out-String)
Assert-LastExitCode 'qmake version check'
if ($qmakeText -notmatch "QMake version $([regex]::Escape($ExpectedQmakeVersion))") {
    throw "Expected qmake $ExpectedQmakeVersion, got:`n$qmakeText"
}
if ($qmakeText -notmatch "Qt version $([regex]::Escape($ExpectedQtVersion))") {
    throw "Expected Qt $ExpectedQtVersion, got:`n$qmakeText"
}

if (Test-Path -LiteralPath $BuildRoot) { Remove-Item -LiteralPath $BuildRoot -Recurse -Force }
New-Item -ItemType Directory -Path $BuildRoot | Out-Null

$project = Join-Path $RepoRoot 'GT-10FxFloorBoard.pro'
$buildScript = [IO.Path]::ChangeExtension([IO.Path]::GetTempFileName(), '.cmd')
try {
    @(
        '@echo off',
        "call `"$vcvars`" -vcvars_ver=$ToolsetVersion",
        'if errorlevel 1 exit /b %errorlevel%',
        "cd /d `"$BuildRoot`"",
        "`"$qmake`" -early QMAKE_MSC_VER=1929 QMAKE_MSC_FULL_VER=$qmakeMscFullVer -before `"$project`" -spec win32-msvc CONFIG+=release",
        'if errorlevel 1 exit /b %errorlevel%',
        'nmake.exe',
        'if errorlevel 1 exit /b %errorlevel%'
    ) | Set-Content -LiteralPath $buildScript -Encoding ASCII
    & cmd.exe /d /c $buildScript
    Assert-LastExitCode 'qmake/nmake build'
} finally {
    if (Test-Path -LiteralPath $buildScript) { Remove-Item -LiteralPath $buildScript -Force }
}

$exe = Join-Path $BuildRoot 'packager\GTLabEditor.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Release executable not found: $exe" }

$gitCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
Assert-LastExitCode 'git rev-parse HEAD'
$gitBranch = (& git -C $RepoRoot branch --show-current).Trim()
Assert-LastExitCode 'git branch --show-current'
if (-not $gitBranch) {
    $gitBranch = if ($env:GITHUB_REF) { $env:GITHUB_REF } else { 'detached-head' }
}
$compilerVersion = ([regex]::Match($compilerText, '\b(19\.29\.[0-9.]+)\b')).Groups[1].Value

$buildInfo = [ordered]@{
    product = 'GT Lab Editor'
    version = '1.0.0'
    commit = $gitCommit
    ref = $gitBranch
    timestampUtc = [DateTime]::UtcNow.ToString('o')
    qt = $ExpectedQtVersion
    qmake = $ExpectedQmakeVersion
    msvcToolset = 'v142'
    compiler = $compilerVersion
    windowsSdk = $env:WindowsSDKVersion.TrimEnd('\')
    configuration = 'Release x64'
    executable = $exe
}
$buildInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $BuildRoot 'build-info.json') -Encoding UTF8

Write-Host "BUILD PASS: $exe"
Write-Output $exe
