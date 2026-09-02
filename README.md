# GT Lab Editor

**Modern editor for the BOSS GT-10.**

GT Lab Editor is an independent open-source editor and librarian for the
BOSS GT-10. It is a cross-platform project maintained by Wumadson Cardoso /
GT LAB. The current version is **1.0.0**.

Project repository: <https://github.com/wumadson/GTLabEditor>

## Requirements

- BOSS GT-10
- USB connection to the computer
- Windows 10 x64, Windows 11 x64, or a supported macOS version

On Windows 10, the independently installed Roland/BOSS driver is recommended.
On Windows 11, GT LAB WinUSB provides editor/MIDI communication; it does not
provide GT-10 USB audio.

## Overview

GT Lab Editor communicates with the BOSS GT-10 through MIDI and SysEx. Its
modern interface includes patch management, Signal Chain editing, Quick
Settings, System controls, and English and Brazilian Portuguese UI support.

The project targets Windows 10, Windows 11, and macOS. Platform validation and
hardware requirements may differ as described below.

## Windows 10

The preferred Windows 10 connection uses the official Roland/BOSS GT-10
driver. MIDI communication is provided through WinMM/RtMidi, while the
driver's GT-10 USB audio and ASIO facilities remain available according to the
driver and operating-system configuration.

The Roland/BOSS driver is not distributed by this project. Obtain any required
driver from an official Roland/BOSS source and follow its documentation.

## Windows 11 and GT LAB WinUSB

GT Lab Editor includes a GT-10 WinUSB backend for editor and MIDI
communication. It has been physically validated on Windows 11 x64 with
HVCI/Memory Integrity enabled. It uses the Microsoft inbox WinUSB driver and
does not require disabling Windows security features.

The backend already exists and has been physically validated. What remains
future work is its final signed public packaging. The public installer will
offer the WinUSB component only after explicit user confirmation. Activating
WinUSB changes the driver binding for the GT-10 USB device. While that binding
is active:

- native GT-10 USB audio is unavailable;
- the Roland ASIO driver is unavailable;
- the Roland mixer interface is unavailable.

### Audio and ASIO limitation

GT LAB WinUSB currently provides editor/MIDI communication only. The original
GT-10 USB audio/ASIO functionality is not available while the WinUSB binding
is active.

For DAW recording in this mode, use a dedicated external audio interface.

Do not disable HVCI, Memory Integrity, Secure Boot, or driver-signature
enforcement to install GT Lab Editor.

## macOS

The application uses the existing CoreMIDI-based transport on macOS. The
Windows WinUSB backend and Windows version resources are not used on macOS.

## Installation and uninstallation

On Windows 10, GT Lab Editor can be installed normally and used with an
independently installed Roland/BOSS driver.

For Windows 11, a future public installer may offer the GT LAB WinUSB component
with a clear description of the driver change and its audio limitation before
making any system change. Laboratory certificates and laboratory driver
packages are not public release components.

The Windows installer registers GT Lab Editor in Apps & Features / Add or
Remove Programs. Uninstallation removes the installed GT LAB application. If a
public WinUSB component is installed, its uninstall path must remove that
component and allow Windows to restore an appropriate previously available
driver binding, without deleting a Roland driver package from the Driver
Store. User preferences may be retained intentionally for a later reinstall.

## Original project

GT Lab Editor is derived from the **GT-10 FxFloorBoard / FXFloorBoard**
project.

Original development:

- Colin Willcocks
- Uco Mesdag

Original source code:
<https://sourceforge.net/p/fxfloorboard/fxfloorboard/ci/gt-10/tree/>

The Git history, source notices, credits, and copyright statements from the
original project are preserved. GT LAB does not claim authorship of the
original FXFloorBoard code.

Modernization and additional development:

- Wumadson Cardoso / GT LAB

## License

GT Lab Editor is distributed under the **GNU General Public License version 2
or, at your option, any later version (`GPL-2.0-or-later`)**. The normative
license declaration is provided by the source headers, project notices, and
[LICENSE.GPL-2.0](LICENSE.GPL-2.0).

The historical [`license.txt`](license.txt) contains the complete GNU GPL
version 3 text. Its presence does **not** change the project to
`GPL-3.0-only`: the source's `GPL-2.0-or-later` grant permits recipients to
choose GPL version 2 or a later GPL version, including GPLv3. The historical
file is retained for provenance and compatibility with earlier distributions.

The project may be redistributed commercially, subject to the terms of the
GPL. Recipients retain all rights granted by the GPL. Binary distributions
must satisfy the GPL requirements, including access to the complete
corresponding source and applicable build scripts.

Third-party components retain their respective licenses and notices. See
[Third-Party Notices](installer/THIRD-PARTY-NOTICES.txt) and
[Asset Provenance](ASSET-PROVENANCE.md).

Relevant components include Qt 5.15.2, RtMidi, OSDaB XMLWriter,
Qtractor-derived material, Microsoft Visual C++ runtime components, and
ANGLE/Qt runtime components. Upstream binaries must not be represented or
signed as binaries authored by GT LAB.

## Privacy

This program will not transfer any information to other networked systems
unless specifically requested by the user or the person installing or
operating it.

GT Lab Editor contains no telemetry, analytics, or automatic data upload.
External Help, Manual, Source Code, Web, and Donate URLs are opened only in
response to a user action.

## Project maintenance

The current repository is maintained by Wumadson Cardoso / GT LAB. The
original project's history and credits remain part of this derived project.
Contributions are handled through GitHub. The intended release-signing process
requires manual approval for each release; the verifiable CI and signing
workflow are not yet implemented.

### Project roles

- **Authors / Committers:** Wumadson Cardoso / GT LAB
- **Original project authors:** Colin Willcocks and Uco Mesdag
- **Reviewers:** TBD before the SignPath application
- **Approvers:** TBD before the SignPath application

No additional people are implied by the provisional `TBD` entries. Named
reviewers and approvers, repository MFA, and SignPath MFA require manual
confirmation before applying.

## Code signing policy

GT Lab Editor is preparing an application to the SignPath Foundation. The
project has not yet been approved, and current release artifacts must not be
described as SignPath-signed.

The policy intended after project approval is:

> Free code signing provided by SignPath.io, certificate by SignPath Foundation.

Only artifacts built verifiably from this repository may be submitted. Each
signing request must receive manual approval. Project-owned artifacts intended
for signing are `GTLabEditor.exe` and the GT Lab Editor Windows installer.
Upstream Qt, ANGLE, Microsoft runtime, Windows system, and other third-party
binaries are not to be signed as GT LAB-authored binaries.

Until SignPath approves the project and the verifiable build workflow is in
place, this section records an intended policy rather than an active signing
service.

## Trademark disclaimer

BOSS and GT-10 are trademarks or registered trademarks of their respective
owners. GT Lab Editor is an independent project and is not affiliated with,
endorsed by, or sponsored by Roland Corporation or BOSS.
