# MMMerge icon ownership for the Apple import work

Checkpoint 085 preserves original palettes and pixel indices for all 910 typed inventory images
and IFT icon frames. Both stages produce identical bytes for their shared images; existing owner
and collision rules remain unchanged. Portrait frames and other callers keep RGB output, and wrapped
PCX stays unchanged. All 5,002,938 item RGB pixels and source indices/palettes match independently;
241 additional IFT images change encoding with identical RGB. Actual Mac shop hammer rendering and
all 7,701 pixel hit tests pass. See ITEM_TRANSPARENCY_VALIDATION.md for scope and remaining work.

Checkpoint 083 imports 1,062 original fitted equipment images from 1,107 typed references under engine/icons.
Only icons.lod to mm8.icons.lod, and mm6.icons.lod to mmmerge.icons.lod, may resolve competing owners
in this stage. Other collisions fail with rollback; lowercase and 0644 remain required. All 19,051,614
RGB pixels match independent original decoding. Forty-five missing collar-b references remain reported.
Shared variants preserve armor poses, original complex flags and permitted doll-specific cloaks.
Four actual Mac views and a normal armor/sword round trip pass limited acceptance; a two-handed staff
arm seam, edge clipping, all-race coverage and complex front-collar wearing remain open.
See EQUIPMENT_VISUAL_VALIDATION.md; 082 below is the earlier body-image checkpoint.

Checkpoint 082 imports 491 original body, hand, head and background images into engine/icons,
selected from nine typed columns of all 77 original character rows (502 unique references).
The only permitted competing owner is the original icons.lod followed by mmmerge.icons.lod;
pc22rhd uses that explicit Merge patch. Other collisions, corrupt images and escaping archives
fail with the entire new doll stage rolled back. Names are lowercase and files are 0644.
All 4,035,869 RGB pixels match independent original LOD decoding. Eleven source references remain
missing; the current four companions have every typed body reference. Actual Mac captures now show
their bodies/hands after correcting original downward-positive coordinates. Equipment pose variants,
shield placement and complete visual acceptance remain open. See PAPERDOLL_VALIDATION.md.

Checkpoint 079 adds 963 global object sprites under engine/sprites and 12 palettes under engine/textures.
Owners use the existing explicit scene archive layers; no arbitrary override order is inferred.
All referenced directions/animation frames are collected from unchanged object/SFT IDs. Original null
sentinels remain reported. World skies belong physically under worlds/mm*/textures; sky_textures is
a virtual scale category, not a package directory. All original pixels match and MM8 clouds render.
See DYNAMIC_PRESENTATION_VALIDATION.md for the source ledger and remaining gameplay validation.

Historical checkpoint 078 adds world-local MM8 minimaps and direction arrows. For selected MM8 names only,
a known mm8/mmmerge/00 patch icon layer may replace the original icons.lod owner; unrelated owners
or competing overlays still fail. Actual out02/out13 select mmmerge.icons.lod. All 14 minimaps and
8 arrows retain their source pixels and lowercase/0644 output; 3,663,240 pixels were compared.
Existing engine/MM6/MM7 entries are unchanged. See MM8_SCENE_VALIDATION.md for the outstanding
dynamic sky/projectile/splash images; imported map references alone do not prove complete artwork.

Checkpoint 077 adds item-table-driven images to shared engine/icons. ItemIconResources runs inside
the inventory stage and imports910source images with unique owners:314originalMM8,258MM6,301MM7,
17prefixedMM8 and20Merge. Filenames are lowercase and files0644; each name records itemIDs, archive,
dimensions and outputpath. Conflicting selected owners fail; arbitrary mod override order is not inferred.
The literal null reference and blank item0 icon remain reported. Of910images,901are new package entries
and9already existed identically. All5,002,938RGBpixels match source and every previous package entry
is unchanged. Actual Mac chest contents, mouse/touch-marked transfer and Chinese inspection nowpass;
physicaliPad and allinventory/shop visuals remainpending. See ITEM_ICON_VALIDATION.md.

Checkpoint 076 adds the extracted original MM8 base. All eight original chest backgrounds now
import into shared engine/icons with the existing service precedence and lowercase/0644 rules.
Shared frame selection lets known mm6/mm7/mm8/mmmerge icon/D layers, 00 patch.icons.lod,
z10 loczhcn.icons.lod, icons.ui.lod and patch.icons.ui.lod override original icons.lod independently
of enumeration order. Unknown or competing overlay owners still fail; each image records its owner.
Original NPC pictures are not added again because the prefixed MM8 archive already supplies their
merged IDs. Item images such as cndlabra, herb03 and item204–206 exist in source but remain unpackaged;
the chest background is visible while contents are not yet visually accepted. See MM8_BASE_RUNTIME_VALIDATION.md.

Checkpoint 059 adds an authored global Chinese turn-phase/movement label under engine/ui/gameplay,
using existing solid-color primitives and text. Original turn icons remain unchanged and their
missing source is not replaced with another image. The eight missing shared Ice Bolt sprite names
have no exact match in 41 original LOD indices; similarly named MM7-prefixed images are not remapped.
No table or icon ownership changes. See HOSTILE_TURN_COMBAT_VALIDATION.md.

Checkpoint 057 keeps spellbook presentation globally owned under engine/ui/gameplay. Its authored
Chinese text controls use engine solid-color UI primitives and the existing original book pages; no
spell artwork is synthesized, remapped or moved into worlds. The missing icon set remains a source
gap, recorded by checkpoint 056. The two obsolete attack-button images gf-aspell/gf-aspelld are no
longer referenced or packaged by this layout; their source files remain untouched. All other resource
payloads retain their 054 bytes and ownership. See SPELLBOOK_CONTROLS_VALIDATION.md.

Checkpoint 052 imports original 6mapdir1–8 and 7mapdir1–8 under their world's icons, preserving original
filenames, RGB pixels and direction order. World ui/minimap_arrows.txt files bind the shared direction
roles explicitly; missing images cannot resolve from another world. Qualified original arrow filenames
retain their black transparency key. No global gameplay-table ownership is changed.
The packaged world namespace mount is corrected using independent streamed archive I/O.
See MINIMAP_NAVIGATION_VALIDATION.md; the 051 unbound-arrow note below is historical.

Checkpoint 051 adds the 28 original outdoor map-stem images to their registered world's icons directory
(15 MM6, 13 MM7). They are map-local presentation, not engine-owned party assets. outc1 is owned by
mmmerge.icons.lod; all other sources are the matching mm6/mm7.icons.lod. Duplicate owners fail explicitly;
missing images stay reported. MAPDIR party arrows remain unbound: source images use 6mapdir/7mapdir names.
See MINIMAP_SOURCE_VALIDATION.md. Earlier missing-minimap statements below are historical.

Checkpoint 050 adds 2,465 original character-pose images from top-level mm8.icons.lod and
mmmerge.icons.lod to engine/icons. Character prefixes plus PFT indices own these references globally;
they are not duplicated per world. Missing 1,623 poses / 29 normal portraits remain reported.
Five wide-HUD regions reference the selected original Basebar/scoreBG images through authored layout
metadata; the runtime reads their pixels, and no derivative BMP files or substitute images are exported.
The authored skin's source dimensions and transparency are checked. See HUD_SOURCE_VALIDATION.md.

Checkpoint 049 adds 28 referenced hardware-liquid frame images to worlds/mm6/textures and
14 to worlds/mm7/textures, using original LOD images and normal patch precedence. All frame
ownership is recorded per map in the scene report. Shared source-derived material definitions
stay in engine/rendering. The captured MM7 water defect is fixed; wide-HUD artwork remains
missing. No source gap is replaced with generated artwork. See HARDWARE_WATER_VALIDATION.md.

Checkpoint 048 adds MM7 surfaces/palettes, actor/decor/object sprites and available skies
under worlds/mm7, with shared definitions still engine-owned. The pipeline has 140 scenes.
No substitute artwork is introduced. Actual Mac captures reveal missing authored wide-HUD
resources (basebar_cut, status_bar and score backgrounds); the importer reports these gaps.
See MM7_GAMEPLAY_VALIDATION.md. Earlier checkpoint counts below are historical.

Checkpoint 047 expands to 64 MM6 maps. Scene surfaces/palettes stay in worlds/mm6/textures,
actor/decor/object images in worlds/mm6/sprites, and available source skies in
worlds/mm6/sky_textures (corrected to physical textures in 079). SFT/TFT and terrain tables remain engine-owned.
No global icon ownership changes or replacement artwork are introduced. Scene texture/sprite
provenance and exact missing references appear in reports/scene/scene_conversion.json;
see SCENE_IMPORT_PROFILE.md. Earlier three-dungeon measurements below are historical.

Checkpoint 045 (2026-09-11) adds world-owned indoor surface textures, monster/decor/object
sprites and palettes for MM6 6d01/6d02/6d18. Resources stay under worlds/mm6/textures and
worlds/mm6/sprites; raw maps stay under worlds/mm6/maps. Common TFT/SFT definitions remain
engine-owned. Per-image archive provenance and missing art appear in
reports/indoor/indoor_conversion.json. No substitute art or new global icon ownership is added.
The missing dec04 campfire frames and pal008 are explicitly reported for 6d01/6d02.


This records images promoted by the native Apple import converters. It is not an
inventory of the entire upstream asset collection.

Checkpoint 044 (2026-09-11) assembles the existing image converters into native
runtime packages without changing their ownership or adding substitute art.
The selected UI set may replace generic frame images under engine/icons;
other differing overlaps fail conversion. ZIP entries retain lowercase resource
paths and 0644 permissions. Reports remain separate by converter stage.
See RUNTIME_IMPORT_PROFILE.md; missing original art is still unresolved.

## Native NPC media checkpoint (2026-09-10)

`MediaResources.cpp` now imports 1,646 original NPC portraits. Exactly four-digit
`npcNNNN` members are selected; player face sequences and paperdolls are separate.

| Source | Count | Output ownership |
| --- | ---: | --- |
| `Data/mm6.icons.lod` | 524 | `worlds/mm6/icons/npcNNNN.bmp` |
| `Data/mm7.icons.lod` | 504 | `worlds/mm7/icons/npcNNNN.bmp` |
| `Data/mm8.icons.lod` | 450 | `worlds/mm8/icons/npcNNNN.bmp` |
| `Data/mmmerge.icons.lod` | 168 | `engine/icons/npcNNNN.bmp` |

There are no duplicate selected IDs in the supplied installation; conflicts are
rejected rather than resolved with an undocumented archive order. Outputs are
lowercase/0644. All 7,568,012 RGB pixels match independent source decoding. All
163 nonzero proprietor portrait references resolve. Formal Mac Metal rendering
shows the tavern owner, Andover and Maria alongside Chinese labels and dialogue.
See `MEDIA_IMPORT_PROFILE.md` for screenshots, source provenance and limitations.
The following sections retain the scope of each earlier converter.

`tools/mmerge_import/MenuResources.cpp` imports the shared title and menu buttons
into the generated preview's `engine/icons/` directory. They are base presentation
assets, so no world package receives a duplicate. All output names are lowercase
and generated resource files use mode 0644.

| Resources | Output | Ownership and override order |
| --- | --- | --- |
| `title.pcx` | `engine/icons/title.pcx` | Shared base menu; preserve wrapped PCX payload |
| `t_new`, `t_load`, `t_cred`, `t_quit`, each with `_up`, `_ht`, `_dn` | `engine/icons/<name>.bmp` | Shared base menu; decode indexed image and palette |

For these thirteen images only, later archives override earlier archives:
`Data/mm8.D.lod`, `Data/mmmerge.D.lod`, then optional
`Data/10 LocZHCN.EnglishD.lod`. Each chosen archive is recorded in the generated
`conversion.json`. In the supplied installation, all thirteen images came from
the Chinese overlay. The Mac and physical iPad manifests matched.

This explicit selection does not resolve arbitrary mod ordering. NPC portraits,
inventory icons, paperdolls and other image classes have not been imported by this
converter. Original-world NPC portraits must remain world-owned when added later.
Generated previews retain `runtime_ready=false`; no gameplay tables are promoted.

## Service converter: chest backgrounds

`ServiceResources.cpp` can decode unprefixed `chest01`–`chest08` to shared
`engine/icons/chest01.bmp`–`chest08.bmp`. Its archive priority is base `icons.lod`,
then `mm8.D.lod`, `mmmerge.D.lod`, and optional `10 LocZHCN.EnglishD.lod`.
It records selected owners and dimensions, validates grid bounds, and retains
lowercase/0644 output rules. Damaged present images roll back the output.

All eight backgrounds are missing in the supplied installation and are listed in
`service_conversion.json`. This real-source round adds no chest image assets.
The available prefixed MM6/MM7 chest pictures remain world-owned and are not
silently substituted into the inspected MM8 layout. Interactive chest readiness
remains false. See `SERVICE_IMPORT_PROFILE.md` for coordinate and ownership evidence.

## Frame converter: shared interface animations

`FrameResources.cpp` adds 36 original images referenced by the global IFT table:

| Resources | Output | Selected owner |
| --- | --- | --- |
| `fr1`–`fr30` | `engine/icons/fr1.bmp`–`fr30.bmp` | `Data/mm6.icons.lod`, 30 indexed images |
| `aframe1`–`aframe6` | `engine/icons/aframe1.bmp`–`aframe6.bmp` | `Data/mm7.icons.lod`, six indexed images |

These are shared interface animation assets. Their archive origin does not make
them world NPC portraits; no per-world duplicates are created. Numeric PFT face
indices do not imply character portrait art has been imported. Future original
world NPC portraits must still keep their world ownership.

Only top-level icon/presentation LODs are searched, with one unambiguous owner
required per selected name. A conflict fails conversion until a specific override
profile exists. Sprite archives, including same-named members in another format,
and nested conditional Additional UI packs are excluded. The decoder validates
image format and dimensions; outputs keep lowercase names and mode 0644.

The manifest records archive ownership and all 371 missing nonempty references;
three original blank texture cells are recorded separately. Every exported RGB
pixel matches independent source decoding, but rendered transparency and complete
animation presentation remain unaccepted. No original source is modified.

## Arcomage converter: original card art and table

`ArcomageResources.cpp` exports two shared game images to `engine/icons/`, with
lowercase names and mode 0644. They belong to the global card game even when the
archive containing an image originated in MM7. No per-world copy is created.

| Resource | Output | Selected source in the supplied installation |
| --- | --- | --- |
| `sprites.pcx` | `engine/icons/sprites.pcx` | `10 LocZHCN.EnglishD.lod`, Chinese 960×2500 card/UI atlas |
| `layout.pcx` | `engine/icons/layout.pcx` | `mm7.icons.lod`, 640×480 table background |

For these two names, archive priority is `mm7.icons.lod`, `mm8.D.lod`,
`mmmerge.D.lod`, then optional `10 LocZHCN.EnglishD.lod`. Both PCX payloads remain
byte-identical; native BGRA decoding also matches independent Pillow decoding.
The existing Sprites/Layout runtime names resolve to these lowercase files.
Actual card-screen rendering, transparency and interactive input remain pending.
See `ARCOMAGE_IMPORT_PROFILE.md` and `arcomage_conversion.json` for the exact owners.

## Gameplay UI converter: original MM6 interface skin

`UiResources.cpp` selects set 3 from the supplied Additional UI table for the
current MM6 checkpoint. All 256 images are shared interface art and live in
`engine/icons`; the source archive's MM6 origin does not make the common HUD
world-local. Files preserve lowercase names and mode 0644. No character portraits
or missing art are invented. Font payloads use the same shared lookup directory.

The selected skin's `butn4u/d` options images are present 1x1 placeholders.
The authored HUD now adds a `Menu` text label at the existing options hit target;
the source images, lookup priority and ownership remain unchanged. This is an
OpenYAMM layout label, not newly generated or substituted original art.

| Selected source | Images in the checkpoint 050 output |
| --- | ---: |
| `Data/Additional UI/6UI.icons.lod` | 85 |
| `Data/Additional UI/6UI.EnglishD.lod` | 43 |
| `Data/10 LocZHCN.EnglishD.lod` | 88 |
| `Data/icons.UI.lod` | 6 |
| `Data/00 add.icons.lod` | 2 |
| `Data/mmmerge.icons.lod` | 25 |
| `Data/z10 LocZHCN.icons.lod` | 7 |

The explicit namespace/override rules are in `UI_IMPORT_PROFILE.md`. Every image
records its complete relative archive path. The 8,766,844 RGB pixels independently
match their originals. Actual Mac rendering shows the standard HUD frame and
bars; complete transparency, layout, portraits, alternate skins and iPad
interaction are still unaccepted. All 319 absent static references are reported.

The authored `town_portal.yml` also activates a validated dynamic Dimension
Door resource set. `TPGlobal` is the common background; daily routing selects
one of `TPJadam1` through `TPJadam8`, `TPAntag1` through `TPAntag8`, or
`TPEnroth1` through `TPEnroth5`. All 22 members in the supplied installation
are owned by `Data/mmmerge.icons.lod` before any later standard icon override.
They remain shared `engine/icons` assets because the same portal overlay chooses
among the three worlds at runtime. Missing members are listed separately in
`ui_conversion.json`; no world-local duplicate or substitute is created.

## New Sorpigal decorations (2026-09-09)

`DecorationResources.cpp` imports 53 original indexed sprite images into
`worlds/mm6/sprites` and five source palettes into `worlds/mm6/textures`.
These are map presentation assets; the 673-entry decoration registry and full
SFT remain global under `engine/`. All 479 map names resolve, and all required
visible decoration images/palettes are present. Sprite archive ownership is
recorded in `decoration_conversion.json`. Independent comparison verifies
1,824,349 index pixels and palette bytes. No images are invented or repurposed
as missing portraits. Lowercase/0644 rules apply. See `DECORATION_IMPORT_PROFILE.md`.
