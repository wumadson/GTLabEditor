# Asset provenance

This inventory records the evidence currently available for graphical assets
used by GT Lab Editor. It is intentionally conservative: an asset is marked
`UNKNOWN / NEEDS REVIEW` when the repository history does not prove its author,
source, or license.

| Path / group | Origin | License | Status | Notes |
|---|---|---|---|---|
| `assets/menu/*.png` | Added by Wumadson Cardoso in the GT Lab Editor modernization (`074bfb0`) | Added as part of the GPL-2.0-or-later project; explicit per-asset licensing/provenance record pending | GT LAB / REVIEW DOCUMENTATION | Fourteen clean menu icons created for the modern UI. No external icon package is recorded in the commit history. |
| `assets/effects/*.png` | Added during GT Lab Editor modern-UI commits by Wumadson Cardoso | Added as part of the GPL-2.0-or-later project; explicit per-asset licensing/provenance record pending | NEEDS REVIEW | Confirm for every file whether it was drawn for GT LAB, derived from a legacy image, or based on manufacturer artwork. |
| `assets/pedals/*.png` | Added during expression/pedalboard modernization by Wumadson Cardoso | Added as part of the GPL-2.0-or-later project; explicit per-asset licensing/provenance record pending | NEEDS REVIEW | Confirm source artwork and any derivation. |
| `assets/pedalboard/*.png` | Added during pedalboard modernization by Wumadson Cardoso | Added as part of the GPL-2.0-or-later project; explicit per-asset licensing/provenance record pending | NEEDS REVIEW | `bossgt10.png` and `gt10_footswitch.png` may depict BOSS hardware. Review trademark and product-depiction considerations separately from copyright and provenance of the actual artwork, photograph, panel graphic, or logo. No infringement conclusion is implied. |
| `GTLabEditor.png`, `GTLabEditor.ico`, `GTLabEditor.icns` | Added in GT Lab Editor identity/icon commits by Wumadson Cardoso | Added as part of the GPL-2.0-or-later project; explicit per-asset licensing/provenance record pending | GT LAB / REVIEW DOCUMENTATION | Preserve editable source/provenance evidence if available. |
| `images/*` | Inherited primarily from GT-10 FxFloorBoard commits by Colin Willcocks, 2008-2010 | Historically distributed with the FXFloorBoard GPL source tree, but no per-asset license declaration was found | UNKNOWN / NEEDS REVIEW | Audit each runtime-used image against the original SourceForge tree and historical release notices. Historical inclusion in a GPL source tree does not by itself prove the license of every individual asset. |
| `images/qt-logo.png` | Legacy repository asset representing Qt | Source and redistribution terms not recorded beside the file | UNKNOWN / NEEDS REVIEW | Prefer an official Qt asset with documented trademark/license terms or remove it from release use if unnecessary. |
| `images/GT-10FxFloorBoard.png`, `images/floor.png`, `images/floor.psd` and GT-10 imagery | Legacy FXFloorBoard artwork | Per-file origin and any Roland/BOSS source material are not documented | UNKNOWN / NEEDS REVIEW | Review trademark and product-depiction considerations separately from copyright and provenance of each photograph, panel graphic, logo, or other source artwork. No infringement conclusion is implied. |
| `images/*.psd` | Legacy editable artwork | No per-file declaration | UNKNOWN / NEEDS REVIEW | Review whether any PSD is required as the preferred form for modification for GPL source-completeness purposes, and whether embedded elements have separate rights. |
| `packager/GT-10FxFloorBoard.app/Contents/Resources/GT-10FxFloorBoard.icns` | Legacy macOS package | No per-file declaration | UNKNOWN / NEEDS REVIEW | Not the current GT Lab application icon; review/remove from future release inputs only in a separate cleanup. |
| `packager/GT-10FxFloorBoard_help_files/image*.jpg` | Legacy software help package | No per-file declaration | UNKNOWN / NEEDS REVIEW | Likely documentation screenshots; confirm original-project provenance. |

## Classification rules

- **GT LAB:** repository history identifies a GT Lab modernization commit and
  no external source is recorded. Supporting design/source records are still
  desirable.
- **NEEDS REVIEW:** some provenance is known, but authorship, derivation, or
  licensing is not sufficiently documented for a public binary release.
- **UNKNOWN / NEEDS REVIEW:** no reliable per-asset provenance or license was
  found. This is not an assertion of infringement; it is an unresolved release
  checklist item.

## Required follow-up before public release

1. Record the creator and source of each modern asset, including editable
   originals where available.
2. Compare legacy runtime assets with the original FXFloorBoard SourceForge
   tree and retain evidence that they were distributed with the GPL source.
3. Review assets depicting BOSS/GT-10 products or logos for trademark and
   artwork permissions.
4. Document the Qt logo's official source and permitted use, or remove it from
   the release UI if it is not required.
5. Prefer replacing or excluding unresolved assets from signed public
   artifacts until provenance is resolved.
