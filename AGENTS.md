# AGENTS.md
# GT-10 FxFloorBoard Modern

## Project purpose

This repository is a modernization fork of **GT-10 FxFloorBoard** for the **BOSS GT-10**.

Primary goal:

- preserve the proven MIDI/SysEx engine and GT-10 parameter logic from the original FxFloorBoard;
- make the application compile and run natively on modern macOS / Apple Silicon;
- progressively replace the legacy Qt4-era visual interface with a modern, responsive and friendlier Qt interface;
- keep compatibility with the separately developed **GT-10 USB Bridge**, which exposes the GT-10 to modern macOS through CoreMIDI and CoreAudio.

Prefer reuse of the existing engine and parameter mappings. Do not turn this into a full rewrite unless there is a strong technical reason.

## Development environment

Validated environment:

- macOS 26 Tahoe
- Apple Silicon / ARM64
- Apple clang
- Qt 5.15.19 via Homebrew
- qmake project
- build file: `GT-10FxFloorBoard.pro`
- original SourceForge branch: `gt-10`
- current modernization branch: `modern-ui`

Expected qmake:

```bash
/opt/homebrew/opt/qt@5/bin/qmake
```

Typical build:

```bash
cd ~/Documents/GT10-FXFloorBoard-Modern
/opt/homebrew/opt/qt@5/bin/qmake GT-10FxFloorBoard.pro
make -j$(sysctl -n hw.ncpu) 2>&1 | tee /tmp/gt10-modern-build.log
grep -n "error:" /tmp/gt10-modern-build.log | head -30
```

Generated application:

```text
packager/GT-10FxFloorBoard.app
```

Launch:

```bash
open packager/GT-10FxFloorBoard.app
```

## Repository checkpoints

Original source was cloned into:

```text
~/Documents/GT10-FXFloorBoard-Modern
```

Important intended checkpoints/tags:

```text
baseline-gt10-original
baseline-macos-arm64-working
```

Current development branch:

```text
modern-ui
```

Before risky changes:

```bash
git status
git log --oneline --decorate -10
```

Do not casually rewrite or squash the known-working baseline.

## Known working state

Validated with a real BOSS GT-10 physically connected:

- original FxFloorBoard source compiles natively on Apple Silicon;
- application launches;
- MIDI OUT works;
- changing parameters in FxFloorBoard changes parameters on the GT-10;
- MIDI IN from the GT-10 reaches macOS through the GT-10 USB Bridge;
- CTL1 / CTL2 MIDI messages were observed;
- patch-change MIDI messages were observed;
- GT-10 USB Bridge works bidirectionally with CoreMIDI.

Important behavioral observation:

- many knob turns or effect toggles performed directly on the GT-10 do not necessarily emit spontaneous parameter messages;
- this appears to be normal GT-10 / original FxFloorBoard behavior, not evidence of a USB Bridge failure;
- CTL1 / CTL2 and patch changes do transmit MIDI.

Do not assume all front-panel changes should automatically update the editor.

## GT-10 USB Bridge

USB transport for modern macOS is already solved in a separate project.

Validated bridge capabilities:

- CoreMIDI virtual MIDI IN / OUT;
- CoreAudio 2 input / 2 output;
- 44.1 kHz;
- full duplex;
- automatic audio service;
- native Apple Silicon app;
- background operation.

Validated release:

```text
GT-10 USB Bridge v1.1.1
```

Validated installer SHA-256:

```text
db53d8ed6338ffbc45787ce1dfe162bf3d3e16ee3d2a558f39749379b4e02156
```

Treat the USB Bridge as external infrastructure. Do not modify or redesign it from this repository unless explicitly requested.

## Original FxFloorBoard architecture

Core MIDI / SysEx engine:

```text
SysxIO.cpp / SysxIO.h
midiIO.cpp / midiIO.h
MidiTable.cpp / MidiTable.h
sysxWriter.cpp / sysxWriter.h
macosx/RtMidi.cpp
```

These are backend/core and should be changed conservatively.

Legacy application shell:

```text
main.cpp
mainWindow.cpp / mainWindow.h
floorBoard.cpp / floorBoard.h
floorBoardDisplay.cpp / floorBoardDisplay.h
bankTreeList.cpp / bankTreeList.h
floorPanelBar.cpp / floorPanelBar.h
dragBar.cpp / dragBar.h
```

Legacy effect widgets:

```text
stompBox.cpp / stompBox.h
stompbox_od.cpp
stompbox_rv.cpp
stompbox_fx1.cpp
stompbox_fx2.cpp
stompbox_eq.cpp
stompbox_dd.cpp
stompbox_ce.cpp
...
```

Legacy editor controls:

```text
customButton.*
customSwitch.*
customKnob.*
customSlider.*
customDial.*
customControl*.*
editWindow.*
editPage.*
menuPage*.*
```

New UI:

```text
modernFloorBoard.cpp
modernFloorBoard.h
modernTheme.cpp / modernTheme.h
modernWidgets.cpp / modernWidgets.h
```

The Modern UI now has a small presentation layer:

- `ModernTheme` owns the shared palette and application stylesheet;
- `AudioGearPanel` owns the QPainter faceplate, material depth and hardware detailing used by modules;
- `AudioGearKnob`, `AudioGearLed` and `AudioGearSwitch` provide reusable painted audio-control components while retaining normal Qt interaction semantics;
- `EffectModule` composes the painted faceplate and audio controls and owns unavailable, ON/OFF and selected visual states;
- `SignalConnector` and `SignalChainPanel` paint the physical signal path behind the modules;
- `StatusBadge` owns connected/disconnected presentation;
- `modernFloorBoard` remains responsible for layout and binding those visual components to the existing legacy backend;
- the bottom `statusBarWidget` still consumes the original `SysxIO` status signals, but its presentation is compact and no longer nests a second `QStatusBar`.

Keep MIDI addresses, buffer validation and `SysxIO` calls out of the reusable visual components.

## Qt4 -> Qt5 compatibility work already done

The original source was Qt4-era.

`GT-10FxFloorBoard.pro` was updated to use modern Qt modules:

```qmake
QT += core gui widgets xml printsupport
```

Old PowerPC/x86-era macOS build flags were removed or adjusted so qmake produces ARM64:

```text
-arch arm64
```

A compatibility header exists:

```text
qt4compat.h
```

It provides explicit Qt5 widget includes for old code that relied on Qt4 transitive includes.

It must stay guarded so it does not break qmake `moc_predefs.h` generation. A pattern like this was used:

```cpp
#if __has_include(<QApplication>)
...
#endif
```

Known API migrations:

```text
QChar::toAscii()    -> QChar::toLatin1()
QString::toAscii()  -> QString::toLatin1()
```

Known files touched include:

```text
customRenameWidget.cpp
renameWidget.cpp
preferencesPages.cpp
```

Legacy print include:

```cpp
#include <QPrintDialog>
```

was migrated to:

```cpp
#include <QtPrintSupport/QPrintDialog>
```

Additional explicit Qt5 headers were required, including widgets such as:

```text
QDesktopWidget
QPainterPath
QListWidget
QListWidgetItem
QListView
QAbstractItemView
QLabel
QLineEdit
QPushButton
QGroupBox
QGridLayout
QHBoxLayout
QVBoxLayout
...
```

Do not revert these compatibility fixes unless replacing them with cleaner equivalents that preserve buildability.

## Legacy UI status

The original UI compiles but renders badly on modern macOS / Qt5.

Known symptoms:

- broken geometry;
- widget overlaps;
- inconsistent sizing;
- obsolete graphical assets;
- fixed-layout assumptions;
- old stompbox presentation.

This is expected.

Do not spend significant time cosmetically repairing the old UI unless required as a transitional backend component.

## Modern UI status

`modernFloorBoard` currently creates a dark modern layout with:

- top header;
- GT-10 title;
- patch number;
- patch name;
- connection status;
- preset sidebar;
- effect chain;
- cards for COMP, OD/DS, PREAMP, EQ, FX-1, FX-2, DELAY and REVERB;
- large effect-editor area.

It is still primarily a visual shell and is not yet a complete replacement for the old editor.

The Modern UI workstation layout now uses one continuous composition:

```text
Patch Library | Signal Chain
              | Artwork | Parameters | Model Browser shell
              | Expression | Control Assign | Pedalboard | Tuner
```

The structural Model Browser and bottom control regions intentionally contain
no fabricated GT-10 data. Their backend bindings are deferred until separately
validated. Existing effect controls remain connected through `modernFloorBoard`
to the proven `SysxIO` and `MidiTable` paths.

The connection label must reflect `SysxIO::isConnected()`. Do not hardcode a fake connected state.

## Current backend integration strategy

Preferred strategy:

**keep the original backend alive while replacing presentation.**

Temporary architecture:

```text
Modern UI (visible)
       |
       v
Legacy FxFloorBoard backend
       |
       +-- floorBoard
       +-- floorBoardDisplay
       +-- bankTreeList
       +-- stompBox instances
       +-- SysxIO
       +-- midiIO
       +-- MidiTable
       |
       v
GT-10 USB Bridge
       |
       v
BOSS GT-10
```

A hidden legacy `floorBoard` has been experimented with to preserve:

- `floorBoardDisplay`;
- `bankTreeList`;
- stompboxes;
- edit pages;
- autoconnect;
- patch state.

This is transitional.

Long-term, backend responsibilities should be extracted cleanly so a full hidden legacy UI is no longer required.

Do not delete working legacy backend code until replacements are proven.

## MIDI connection behavior discovered

`floorBoardDisplay` contains important session logic.

During construction it reads:

```cpp
QString midiIn =
    preferences->getPreferences("Midi", "MidiIn", "device");

QString midiOut =
    preferences->getPreferences("Midi", "MidiOut", "device");
```

If both are non-empty:

```cpp
autoconnect();
```

`autoconnect()`:

```text
check !SysxIO::isConnected()
check SysxIO::deviceReady()
setDeviceReady(false)
connect SysxIO::sysxReply(QString)
    -> floorBoardDisplay::autoConnectionResult(QString)
send GT-10 identity request via SysxIO::sendSysx(...)
```

On a valid GT-10 identity reply:

```cpp
sysxIO->setDeviceReady(true);
sysxIO->setConnected(true);
emit connectedSignal();
```

Therefore:

- connection truth belongs to `SysxIO`;
- Modern UI should display `SysxIO::isConnected()`;
- do not hardcode connection labels.

## Preferences bug discovered on modern macOS

Original code uses relative paths:

```cpp
QFile("preferences.xml")
```

When launched with macOS `open`, observed current working directory was:

```text
/
```

Confirmed with:

```bash
lsof -a -p <PID> -d cwd -Fn
```

which returned:

```text
n/
```

Therefore old code attempted to read/write:

```text
/preferences.xml
```

The bundled default preferences contain:

```xml
<MidiIn device=""/>
<MidiOut device=""/>
```

so autoconnect never starts when no persistent preferences file exists.

### Modern preference fix

`Preferences.cpp` was changed to use a stable app-data path via:

```cpp
QStandardPaths::writableLocation(
    QStandardPaths::AppDataLocation
)
```

and `QDir`, using a helper similar to:

```cpp
static QString preferencesFilePath()
{
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );

    QDir dir(base);

    if (!dir.exists())
        dir.mkpath(".");

    return dir.filePath("preferences.xml");
}
```

Relative `preferences.xml` accesses were changed to use this helper.

This change compiles successfully and was validated with physical GT-10 hardware:

- MIDI IN / OUT selections were saved under `AppDataLocation`;
- after restarting the application, the hidden legacy backend started autoconnect;
- the GT-10 identity reply was received through GT-10 USB Bridge;
- the Modern UI changed from `NOT CONNECTED` only after the legacy backend confirmed the connection.

## Reverb mapping already discovered

Original Reverb stompbox uses:

```cpp
setButton("0A", "00", "30");
```

Therefore:

```text
REVERB ON/OFF
address = 0A 00 30
```

Original boolean flow converts:

```text
false -> 00
true  -> 01
```

and calls:

```cpp
SysxIO::setFileSource(
    area,
    hex1,
    hex2,
    hex3,
    valueHex
);
```

Use this engine path rather than manually reconstructing BOSS SysEx.

## SysxIO behavior verified in source

`SysxIO::setFileSource(...)`:

1. updates the relevant source buffer;
2. builds the device-change message via `MidiTable::dataChange(...)`;
3. if connected and device-ready:
   - marks the device busy;
   - emits status;
   - calls `sendSysx(sysxMsg)`;
4. if connected but busy:
   - appends the message to `sendSpooler`.

Therefore Modern UI controls can reuse `SysxIO::setFileSource(...)`.

Conceptual Reverb integration:

```cpp
SysxIO *sysxIO = SysxIO::Instance();
QString area;

int current =
    sysxIO->getSourceValue(
        area,
        "0A",
        "00",
        "30"
    );

bool newState = (current != 1);

sysxIO->setFileSource(
    area,
    "0A",
    "00",
    "30",
    newState ? "01" : "00"
);
```

This path was validated on physical hardware: the Modern UI read the Reverb ON/OFF state and successfully toggled the real GT-10 Reverb through the existing `SysxIO::setFileSource(...)` engine path.

## First Modern UI hardware milestone

Validated end-to-end with a physical BOSS GT-10:

1. connect GT-10;
2. start/confirm GT-10 USB Bridge;
3. launch Modern FxFloorBoard;
4. choose correct MIDI IN / OUT;
5. verify preferences persist;
6. restart app;
7. verify autoconnect;
8. verify `SysxIO::isConnected() == true`;
9. Modern UI shows connected state;
10. current Reverb state is read from engine;
11. Modern Reverb ON/OFF changes the real GT-10 Reverb state.

This milestone passed. The Modern UI now implements the Reverb parameters already defined by the legacy editor and `midi.xml`:

- Type;
- Reverb Time;
- Pre Delay;
- Low Cut;
- High Cut;
- Density;
- Effect Level;
- Direct Level;
- Spring Sensitivity, enabled only for the Spring type.

These internal Reverb controls were validated with a physical GT-10: changes made in the Modern UI reached the device, and all displayed parameter values matched the current GT-10 patch after readback.

Initial hardware testing showed that parameter writes reached the GT-10, but the Modern UI initially displayed values from the bundled `default.syx` instead of the GT-10's current patch. The legacy `bankTreeList::connectedSignal()` had its `requestPatch()` calls commented out. The connection flow now requests the current temporary patch buffer after a valid identity reply, and Modern Reverb controls remain unavailable until that device patch has been received and propagated through the legacy `updateSignal()` flow. This correction was validated with the physical GT-10: Type, Reverb Time and the remaining implemented Reverb parameters loaded with the device's actual values.

The Modern header now also has a guarded User-patch WRITE path. It snapshots
logical blocks `00`-`0C`, sends the complete current buffer (including `0D`)
through the existing SysEx transport, then performs a direct User-memory RQ1
readback without changing patches or replacing `fileSource`. Only an exact
match of all returned `00`-`0C` data is reported as verified. Block `0D` is
explicitly excluded because the currently known RQ1 does not return it and the
legacy parser synthesizes it from `default.syx`. Persistent WRITE was physically
validated on User patch U49-1: after leaving and returning to the patch, the
edited parameters remained stored. An isolated RQ1 of U49-1 returned the full
1784-byte reply in 685 ms. The initial automatic verification failure was a
callback-ordering bug: verification connected after `isFinished()` and consumed
the preceding DT1 command's trailing empty `sysxReply`. Verification now waits
for that DT1 completion reply before connecting and issuing the RQ1. The decoder
has regression coverage; the corrected automatic MATCH result still requires a
final physical retest.

The first native Quick Settings pilot is isolated to PREAMP A/B User slots
U01-U10. `QuickSettingCodec` owns the validated `30:<slot-1>` address encoding,
29-byte PREAMP A (`01:10-01:2C`) / PREAMP B (`01:30-01:4C`) payloads and the
shared 12-byte name at `40:24-40:2F`. `QuickSettingService` owns lazy name RQ1,
LOAD into only the selected Temporary Buffer slice, and SAVE followed by exact
effect/name readback verification. Common PREAMP byte `01:00`, the opposite
channel and patch identity are excluded. Codec/address/isolation tests pass
offline; LOAD and SAVE still require final physical GT-10 validation before
being declared hardware-proven.

FX-1 Quick Settings use an opt-in segmented plan and do not migrate the
validated short-payload effects. A logical FX-1 payload is 470 bytes split over
User pages `02:00` (128), `03:00` (128), `04:00` (128) and `05:00` (86), with
the same pages under `60:00` for the Temporary Buffer. Its 12-byte name is
shared by FX type and is addressed from `40:48 + (TYPE * 0x0C)` using Roland
7-bit carry. All four segments must validate before publication or Temporary
application; SAVE verifies all four segments and the type-specific name. The
segmented codec tests pass offline, while FX-1 LOAD/SAVE and FX-2 preservation
still require final physical GT-10 validation.

## Hardware validation status

The physical BOSS GT-10 was available for the first Modern UI milestone and validated connection, current-patch readback, Reverb ON/OFF and all implemented internal Reverb controls. Hardware availability may still vary between development sessions.

Therefore:

- source analysis is allowed;
- UI work can continue;
- compile tests can continue;
- static or automated tests can be added;
- MIDI/SysEx hardware behavior beyond the explicitly validated connection, current-patch readback and complete Modern Reverb editor path must not be declared working until physical validation.

Mark hardware-dependent changes as untested.

## UX direction

Target UI:

- dark modern macOS-friendly appearance;
- clear hierarchy;
- responsive layout;
- no fake photorealistic pedal graphics;
- readable typography;
- modern cards/panels;
- effect chain as primary navigation;
- preset sidebar;
- selected-effect editor;
- visible real connection status;
- prominent patch number and name;
- real GT-10 terminology;
- no invented capabilities.

Conceptual chain:

```text
COMP -> OD/DS -> PREAMP -> EQ -> FX-1 -> FX-2 -> DELAY -> REVERB
```

This is a visual/editor concept. Actual GT-10 structure and routing remain authoritative.

## Product correctness rule

The UI must never invent parameters or capabilities.

Rules:

- if GT-10 has the parameter, expose it;
- if it does not, do not fabricate it;
- use actual MIDI table / SysEx mappings from the original engine;
- preserve correct value ranges;
- preserve effect-specific behavior;
- preserve dual-channel / dual-preamp behavior;
- preserve assigns and system functionality;
- preserve patch-file compatibility where practical.

Original source and real GT-10 behavior are the source of truth.

## Recommended migration order

### Phase 1 - Architecture proof

- stable preferences;
- actual connection indicator;
- backend lifecycle;
- one real effect toggle (Reverb);
- compile cleanly;
- validate on hardware.

### Phase 2 - Effect-chain state

Modern cards should show real engine state for:

- COMP;
- OD/DS;
- PREAMP;
- EQ;
- FX-1;
- FX-2;
- DELAY;
- REVERB.

### Phase 3 - Basic effect editing

For each selected effect:

- on/off;
- type;
- primary knobs;
- correct ranges;
- changes sent through existing engine.

### Phase 4 - Patch integration

- actual current patch number;
- actual current patch name;
- preset list sourced from engine;
- selecting a patch loads the real patch;
- Program Change received from GT-10 can trigger appropriate refresh/request behavior.

### Phase 5 - Advanced editors

- full PREAMP editor;
- dual channel / A-B;
- assigns 1-8;
- master/system;
- FX-1 / FX-2 subtype pages;
- EQ;
- delay;
- reverb;
- patch write;
- bulk functions;
- import/export.

### Phase 6 - Remove hidden legacy UI dependencies

Once Modern UI replacements are proven:

- extract backend-only services;
- remove visual-only legacy dependencies;
- preserve proven `SysxIO` / `MidiTable` / `midiIO`;
- eliminate hidden full `floorBoard` if no longer needed.

## Coding rules for Codex

1. Read relevant classes before modifying.
2. Prefer existing engine APIs over new MIDI/SysEx implementations.
3. Do not modify `SysxIO`, `midiIO`, `MidiTable`, or RtMidi casually.
4. If core-engine modification is required, explain why first.
5. Keep changes small and reviewable.
6. Compile after each meaningful change.
7. Read actual compiler output.
8. Do not hide build failures.
9. Do not claim hardware behavior is validated when GT-10 is unavailable.
10. Preserve the working ARM64 baseline.
11. Avoid mass formatting unrelated files.
12. Do not remove legacy code simply because it looks old.
13. Do not rename large sets of signals/slots unless necessary.
14. Keep Qt5 compatibility unless an explicit Qt6 migration is planned.
15. Prefer modern Qt signal/slot syntax for new code, but do not rewrite all legacy syntax solely for style.
16. New UI should be responsive; avoid fixed pixel layouts where practical.
17. Keep visual style separate from MIDI/device logic.
18. Do not hardcode fake connection state.
19. Do not hardcode fake patch names once engine integration begins.
20. Any guessed parameter address must be treated as unverified; prefer mappings proven by original source.

## Build artifacts

Do not commit generated build artifacts.

Typical ignored content:

```text
.qmake.stash
Makefile
generatedfiles/
release/
qrc_GT-10FxFloorBoard.cpp
packager/GT-10FxFloorBoard.app/
```

Before committing:

```bash
git status
```

Inspect every untracked/generated item.

## Useful diagnostics

Build:

```bash
/opt/homebrew/opt/qt@5/bin/qmake GT-10FxFloorBoard.pro

make -j$(sysctl -n hw.ncpu) 2>&1   | tee /tmp/gt10-modern-build.log
```

Errors only:

```bash
grep -n "error:" /tmp/gt10-modern-build.log | head -40
```

Launch:

```bash
open packager/GT-10FxFloorBoard.app
```

Process:

```bash
pgrep -fl GT-10FxFloorBoard
```

Current working directory:

```bash
lsof -a -p <PID> -d cwd -Fn
```

Architecture:

```bash
file packager/GT-10FxFloorBoard.app/Contents/MacOS/GT-10FxFloorBoard
```

Expected:

```text
Mach-O 64-bit executable arm64
```

## Current highest-priority task

When GT-10 hardware is available:

1. connect GT-10;
2. confirm GT-10 USB Bridge;
3. launch Modern FxFloorBoard;
4. configure MIDI IN / OUT;
5. close app and verify `preferences.xml` is stored in AppDataLocation;
6. reopen;
7. verify `floorBoardDisplay::autoconnect()`;
8. verify `SysxIO::isConnected()`;
9. verify Modern connection label;
10. test Modern REVERB ON/OFF against the real pedal.

Until that test passes, do not expand blindly to every effect.

## Initial Codex instruction

Start a new Codex session with:

```text
Read AGENTS.md completely and inspect the repository before making changes.

This is a modernization fork of GT-10 FxFloorBoard for BOSS GT-10.
The original MIDI/SysEx engine already works with our external GT-10 USB Bridge.

First:
1. inspect git status and current branch;
2. inspect the existing Qt5/macOS ARM64 compatibility changes;
3. inspect modernFloorBoard, mainWindow, floorBoard, floorBoardDisplay, SysxIO and midiIO;
4. summarize the current architecture;
5. identify unsafe or duplicated backend logic;
6. do not modify files yet.

After analysis, propose the smallest next step consistent with AGENTS.md.
```

For implementation sessions:

```text
Implement the next approved step using the existing FxFloorBoard engine wherever possible.

Requirements:
- preserve MIDI/SysEx behavior;
- do not rewrite core transport without need;
- compile after changes;
- inspect and fix compile errors;
- do not claim hardware behavior is validated when the GT-10 is unavailable;
- keep changes small and reviewable;
- update AGENTS.md if architecture or validated behavior materially changes.
```

## Final principle

The project is not simply "make the old interface prettier."

The goal is:

```text
original proven GT-10 engine
+
modern native macOS UI
+
GT-10 USB Bridge
=
a practical modern editor for the BOSS GT-10
```

Preserve what already works.
Modernize what users actually see and interact with.
Validate hardware behavior step by step.
