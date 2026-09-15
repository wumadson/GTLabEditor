# Building GT Lab Editor on Windows

This document describes the versioned, unsigned Windows build used to prepare
GT Lab Editor for verifiable CI and future SignPath integration.

## Pinned toolchain

| Component | Required version |
|---|---|
| Qt | 5.15.2 `msvc2019_64` |
| qmake | 3.1, from Qt 5.15.2 |
| MSVC | v142, `cl.exe` 19.29.x, x64 |
| Windows SDK | 10.0.26100.x |
| Inno Setup | 6.7.3 |
| Configuration | Release x64 |

The scripts fail if the compiler, SDK, or Qt version does not match these
constraints. They do not depend on a developer username or a fixed repository
location.

## Local prerequisites

Install the pinned Qt, Visual C++ v142 x64 build tools, Windows SDK, and Inno
Setup versions. The Inno Setup licensing question identified in the release
audit remains **PENDING**; this pipeline reproduces the currently validated
technical build and does not claim that the release-policy question is
resolved.

Provide tool locations through parameters or environment variables:

```powershell
$env:QT_ROOT_DIR = 'C:\Qt\5.15.2\msvc2019_64'
$env:INNO_SETUP_ROOT = 'C:\path\to\Inno Setup 6'
```

Visual Studio is discovered through `vswhere.exe`. If necessary, pass its
installation directory with `-VsInstallPath`.

## One-command build

From the repository root:

```powershell
.\scripts\windows\build-all.ps1
```

The orchestrator performs these steps:

1. initializes the MSVC v142 x64 environment;
2. verifies `cl.exe`, Windows SDK, qmake, and Qt versions;
3. creates a clean shadow build below `build/windows-release`;
4. runs qmake and nmake;
5. creates a clean deployment staging directory;
6. runs the matching Qt 5.15.2 `windeployqt` and stages the complete VC142
   app-local CRT assembly;
7. copies the confirmed license and notice files;
8. validates mandatory Qt DLLs and `platforms/qwindows.dll`;
9. creates a staging manifest with file sizes and SHA-256 hashes;
10. builds the common unsigned Inno Setup installer;
11. creates the portable ZIP and `SHA256SUMS.txt`.

Individual phases may be run with:

```powershell
.\scripts\windows\build-release.ps1
.\scripts\windows\stage-release.ps1
.\scripts\windows\build-installer.ps1
```

## Outputs

Generated files are deliberately ignored by Git:

- `build/windows-release/packager/GTLabEditor.exe`
- `dist/staging/windows/GTLabEditor/`
- `dist/windows/GTLabEditor-1.1.0-Windows-x64-Portable.zip`
- `dist/windows/GTLabEditor-1.1.0-Windows-x64-Setup.exe`
- `dist/windows/SHA256SUMS.txt`
- `dist/windows/build-manifest.txt`

The manifest records the commit, branch/ref, UTC build timestamp, tool
versions, and the SHA-256 and size of each staged file.

## Official v1.1.0 universal installer

The public v1.1.0 installer is built manually with
`installer/windows/GTLabEditor-Windows.iss`, preserving the universal Windows
10/11 installation model used for v1.0.0. The scripts above and CI still select
`GTLabEditor.iss` (app-only); they are not a substitute for building and validating
the universal installer. The app-only installer and portable ZIP are not public
v1.1.0 release assets.

Both installer definitions currently produce the same Setup filename. Keep their
outputs separate and record the exact `.iss`, source commit and final SHA-256 in
the release manifest to avoid publishing the app-only artifact by mistake.

After building and staging the release executable, invoke Inno Setup 6.7.3
explicitly from the repository root:

```powershell
& "$env:INNO_SETUP_ROOT\ISCC.exe" .\installer\windows\GTLabEditor-Windows.iss
```

The historical WinUSB payload and final install/uninstall helpers must be
available locally under `installer/windows/winusb-lab/`. These packaging inputs
do not belong in public Git. Do not regenerate or modify them for the Editor
version bump. The maintainer confirmed recovery and reproduction of the v1.0.0
universal Setup; its SHA-256 was
`5e13b21c09967a1d8596676ada1cc256e7bdbd663f967ec668716650f3f3321f`.
Verify the recovered inputs before packaging:

| Input | SHA-256 |
|---|---|
| `gt10-winusb.inf` | `57fc8c322a7ea96bdab5db98e770aca4a2bd6828fcde79a857971bd1b38ba14e` |
| `gt10-winusb.cat` | `5ff62b27a436df0ab2d649458cc93839e24d57e3b944dcb35e0fb068a59df85e` |
| `GT-LAB-WinUSB-Lab.cer` | `39ce00a770ab28c9de39dfd92eedecb1ba342a7ba21ae9a78e7d242934af150e` |

Retest explicit WinUSB consent, the audio/ASIO warning, and app-only/full
uninstallation on Windows 11. Windows 10 must retain the Roland/WinMM path.
This preparation does not claim final v1.1.0 artifact validation or public
driver signing approval.

## GitHub Actions

`.github/workflows/windows-build.yml` uses `windows-2022`, fixed action commit
SHAs, Qt 5.15.2, MSVC v142, SDK 10.0.26100.x, and Inno Setup 6.7.3. It runs on
manual dispatch and pushes to `modern-ui`. It uploads unsigned artifacts only
and has `contents: read` repository permission.

The workflow contains no signing key, certificate, password, custom token, or
SignPath secret. It does not create a GitHub Release.

## Verifiability and reproducibility

The process is **source-to-artifact verifiable**: build commands, dependency
versions, staging rules, hashes, and packaging inputs are versioned. It is not
yet guaranteed to be **bit-for-bit reproducible**. Sources of variation include
PE/COFF timestamps, Inno Setup timestamps, ZIP entry timestamps, runner image
updates, and deployment ordering.

## SBOM

SBOM generation remains **PENDING**. No additional SBOM tool is introduced in
this stage. A future change should select and pin a trusted SPDX JSON or
CycloneDX JSON generator and verify that it accurately describes Qt plugins,
ANGLE, the MSVC runtime, and other packaged components.

## Future SignPath flow

The intended two-stage flow, subject to confirmation with SignPath, is:

1. build an unsigned `GTLabEditor.exe` from a release candidate commit;
2. submit that EXE for a SignPath signing request and manual approval;
3. assemble the final installer using the signed EXE and unsigned upstream
   runtime binaries;
4. submit the installer for a second signing request and manual approval;
5. publish only the approved signed artifacts in the GitHub Release.

SignPath APIs, secrets, artifact configuration, and signing workflows are not
implemented here. No speculative `signpath/` schema is added.

## WinUSB driver package

The WinUSB backend is compiled into `GTLabEditor.exe`, but this CI does not
build, install, or sign a public WinUSB INF/CAT package.

**PUBLIC WINUSB DRIVER PACKAGE SIGNING = PENDING**

Laboratory INF, CAT, certificate, installer, and test checklists are excluded
from this process.
