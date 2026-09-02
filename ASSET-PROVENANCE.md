# Asset provenance

This inventory records the evidence available for graphical assets used by
GT Lab Editor. Repository history and explicit maintainer confirmation are
recorded separately from third-party authorization or endorsement. Nothing
here claims that BOSS, Roland, Qt, or another manufacturer sponsors or
endorses the project.

## GT LAB project assets

The maintainer confirms that the following assets were produced specifically
for the GT Lab Editor modernization and were not reused as ready-made images
from the legacy FXFloorBoard project. They are distributed as part of the
GPL-2.0-or-later project. Editable sources and creation records should be
preserved where available.

| Path / group | History | Status | Notes |
|---|---|---|---|
| `assets/menu/*.png` | Added by Wumadson Cardoso in `074bfb0` | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Fourteen generic menu icons. No third-party logo or product artwork was identified during visual review. |
| Retained `assets/effects/*.png` | Added in `a803fde`, `bf02060`, and `061d2bf` | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Generic effect-module artwork created for GT LAB. No direct third-party logo or identifiable branded pedal reproduction was found. Unused `eq.png`, `ns1.png`, and `ns2.png` were removed. |
| `assets/pedals/*.png` | Added in `e19f599` and `bf02060` | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Generic expression-pedal artwork; no third-party logo was identified. |
| `assets/pedalboard/gt10_footswitch.png` | Added in `e19f599` | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Created as an interface element representing a GT-10 footswitch/control. Its use does not imply authorization or endorsement by BOSS or Roland. |
| `GTLabEditor.png`, `GTLabEditor.ico`, `GTLabEditor.icns`, `images/windowicon.png` | Added or replaced in `15cad17` and `df5aee0` | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Current GT LAB application identity. |
| `images/splash.png` | Modernized for GT LAB in `bf02060`; complete current composition confirmed by the maintainer as created specifically for GT LAB | GT LAB PROJECT ASSET — KEEP + DOCUMENT | Current startup splash and GT LAB identity. |

## Upstream legacy assets

The runtime still uses generic interface graphics inherited from the GT-10
FxFloorBoard source history. They were committed by the original project
authors between 2008 and 2010 and remain attributed to that upstream project.
GT LAB does not claim authorship of them.

| Path / group | Origin | Status | Notes |
|---|---|---|---|
| Runtime-used generic `images/*.png` | GT-10 FxFloorBoard / FXFloorBoard history, primarily Colin Willcocks | UPSTREAM LEGACY ASSET — KEEP + DOCUMENT | Knobs, switches, backgrounds, controls, effect panels, and status graphics required by the existing Qt resources. Historical GPL-tree inclusion is provenance context, not proof of rights to unrelated third-party artwork. |
| `images/floor.png`, `images/floor.psd` | Modified in upstream commits from 2008–2010 | UPSTREAM LEGACY ASSET — KEEP + DOCUMENT | The PNG is a runtime resource; the PSD is retained as its likely editable source. |
| `images/slider.psd`, `images/slider_knobbg.psd` | Added by Colin Willcocks in `e1024f2` | UPSTREAM LEGACY ASSET — KEEP + DOCUMENT | Retained as likely editable sources for runtime slider graphics. |

## Removed as obsolete

These assets were removed from the current tree after reference audits found
no active product use. Their history remains available in Git.

| Path / group | Reason |
|---|---|
| `assets/pedalboard/bossgt10.png` | Abandoned concept artwork. It had no source, runtime, installer, staging, or help use; its otherwise-unused QRC entry was removed. |
| `assets/effects/eq.png`, `assets/effects/ns1.png`, `assets/effects/ns2.png` | Not present in the QRC and not loaded statically or dynamically by current source. |
| `GT-10FxFloorBoard.ico` | Legacy icon unused by modern build resources and the current installer. |
| `images/GT-10FxFloorBoard.png` | Legacy thumbnail unused by current source; its otherwise-unused QRC entry was removed. |
| `images/wah.png` | Not in the QRC and not referenced by source, build, installer, or documentation. |
| `images/qt-logo.png` | Unused by current source; its otherwise-unused QRC entry was removed. |
| `images/Channel_split.psd` | Legacy PSD with no QRC, build, runtime, modern installer, or editable-source dependency; confirmed by the maintainer as unnecessary to the modern product. |
| `packager/gt-10packager.nsi`, `packager/GT-10FxFloorBoard_help.html`, `packager/GT-10FxFloorBoard_help_files/`, `packager/mingwm10.dll` | Retired NSIS/MinGW packaging and help infrastructure. None participates in the current qmake, Inno, CI, staging, macOS, or Windows product path. Obsolete Help defaults were removed from both preference templates. |

## Pending review

| Path / group | Status | Required decision |
|---|---|---|
| Any retained legacy image later identified as reproducing third-party product artwork | NEEDS REVIEW | Review trademark considerations separately from copyright and provenance of the underlying artwork. This is not an assertion of infringement. |

## Classification rules

- **GT LAB PROJECT ASSET:** explicit maintainer confirmation and repository
  history identify the asset as created for the modernization.
- **UPSTREAM LEGACY ASSET:** inherited from the original FXFloorBoard history;
  original credits are preserved and GT LAB does not claim authorship.
- **KEEP + DOCUMENT:** evidence supports retention, while supporting creation
  or source records remain desirable.
- **PENDING REVIEW:** a specific provenance or legacy-retirement question
  remains. This is not an assertion of infringement.
