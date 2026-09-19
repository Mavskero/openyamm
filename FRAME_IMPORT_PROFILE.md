# MMMerge portrait and icon frame import profile

Checkpoint 050 additionally reads Character portraits.txt's original Face pics prefix column and
collects PFT-referenced character images. Across 73 prefixes / 4,088 references, 2,465 original images
are imported into engine/icons; 1,623 poses remain missing, including 29 normal portraits. Prefix IDs
and names are preserved; the default pc01-01 remains absent. HUD_SOURCE_VALIDATION.md records actual
Mac captures using available original faces. The earlier IFT-only measurements below are historical.

The native `frame_import` reads the supplied installation directly. It exports
two shared animation tables and available original icon images, without running
Windows or the mod. Inputs remain read-only; an audit export is not required.

| Source | Shared output | Preserved identity |
| --- | --- | --- |
| `Data/Tables/PFT.txt` | `engine/data_tables/portrait_frame_data.txt` | 76 frames, 56 groups including ID zero; numeric portrait group and texture indices |
| `Data/Tables/IFT.txt` | `engine/data_tables/icon_frame_data.txt` | 411 frames, 58 groups including the unnamed placeholder; frame order, group names and image references |

## Source groups and timing

The loose PFT/IFT overrides are authoritative for this profile; archived binary
caches are not selected. Source headers, row shapes, contiguous IFT row IDs,
group markers, unique group identities, safe names and integer ranges are checked.
Noninitial portrait rows retain their zero group ID. IFT names are normalized to
lowercase for the shared runtime and filesystem, without generating new aliases.

The supplied `Scripts/Structs/After/DataTablesSupport.lua` computes group totals
and end bits from `GroupStart`. The converter follows that grouping: flag 4 marks
the first frame and flag 1 marks a frame followed by another in its group. Only
the first frame carries the summed raw duration; continuation totals remain zero.
Raw durations and totals must fit the original unsigned 16-bit storage.

Both existing OpenYAMM loaders multiply these raw durations by eight. Conversion
preserves this contract and validates it without modifying the gameplay clock.
The source struct comments describe PFT and IFT time units differently, while
OpenYAMM uses the same multiplier. Original elapsed-time parity has not been
established: `original_wall_clock_timing_validated=false`. These tests establish
frame selection under the current engine contract, not original-game wall time.

Portrait ID zero and the unnamed, zero-time IFT row zero remain unsearchable
placeholders. There are 55 searchable portrait groups and 57 searchable icon
groups. IFT rows 399, 400 and 401 have blank texture names in the original. Their
nonfirst group-name fields (`sptext01b` through `sptext01d`) are preserved, not
substituted as texture names or promoted into separate animation groups.

Serialization is reopened and compared cell for cell. The production
`PortraitFrameTable` and `IconFrameTable` loaders validate all group identities,
durations, first/last ticks of each reachable frame, one/two loop boundaries and
selection at `UINT32_MAX`. The real source passes 1,306 frame-selection checks.

## Available images and unresolved art

The IFT contains 407 distinct nonempty image references. The supplied top-level
icon/presentation archives provide 36 unambiguous indexed images: `fr1`–`fr30`
from `mm6.icons.lod`, and `aframe1`–`aframe6` from `mm7.icons.lod`. The native LOD
decoder exports them to shared `engine/icons` as lowercase BMP files, mode 0644.
Their source archive and dimensions are recorded in `frame_conversion.json`.
Independent decoding verifies every exported RGB pixel against its original
palette and indices. This is not acceptance of rendered transparency or animation.

The scan includes top-level `icons.lod`, `*.icons.lod`, `*.d.lod`, `icons.ui.lod`
and `*.icons.ui.lod`. Sprite archives and conditional nested Additional UI packs
are excluded. Same-named sprite members have another format and are not image
fallbacks. Multiple eligible owners fail explicitly until an override profile is
defined; the converter does not guess general mod archive priority.

The remaining 371 nonempty image references, including the original `pending`
placeholder, are listed as missing in this scan. The three blank references are
listed separately. No substitute art is generated. Character portrait artwork,
portrait metadata, spell/portrait FX bindings and complete UI rendering remain
unready. Numeric PFT texture indices alone do not supply character faces.
See `MMERGE_ICON_OWNERSHIP.md` for ownership.

## Verification and reproduction

```sh
cmake --build build-apple --target frame_import
build-apple/frame_import /path/to/MMMerge /path/to/new-frame-output
python -m pytest tests/test_frame_resources.py -q
cmake --build build-ios --target openyamm_frame_import
```

Twenty focused tests cover derived fields, selection boundaries, preserved blank
references, original pixels, source preservation, malformed rows, unsafe names,
ambiguous ownership, damaged images and canonical path boundaries. Existing or
source-overlapping outputs are refused; failed conversions roll back. All 223
resource tests pass. The Mac converter links and its library compiles to an
iPadOS arm64 object. No device app was updated.

Actual output: `../local-data/frame-tables-001/`; combined assets:
`../local-data/gameplay-base-011/`. Both tables and all 36 images are added without
overwriting existing assets and match the conversion byte for byte. Evidence:

- `../local-data/validation/frame-source-comparison.json`: independent source
  grouping, every table field, missing references and every original RGB pixel.
- `../local-data/validation/resource-tests-frame.log`, `frame-mac-build.log`
  and `frame-ios-build.log` in the same validation directory.
- `../local-data/validation/gameplay-start-011/startup.log`: formal startup passes
  both frame tables and then stops at required `engine/data_tables/spell_fx.txt`.

Optional `house_animations.txt` remains missing. Complete formal startup and the
playable loop remain unpassed; `runtime_ready=false`,
`character_portrait_art_ready=false` and `fx_event_bindings_ready=false`.
This phase leaves the low-confidence overall 35%–40% estimate unchanged.
