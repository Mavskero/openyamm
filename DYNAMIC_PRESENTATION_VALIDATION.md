# Dynamic object and sky presentation — checkpoint 079

The current complete runtime is `../local-data/validation/dynamic-presentation-079/runtime-002`.
Retain checkpoint 078 as the previous package. All work remains on NVMeSSD, with no iPad or simulator
operations. This fixes the 13 exact image-loading gaps observed in the MM8 starting scene; it does
not establish a complete starting-island playthrough or every effect's behavior.

## Import behavior

The new `object_presentation` stage runs after decoration and before events. It uses the validated
global object table and complete original SFT indices to collect every referenced animation and
direction image, including later-frame palettes and a nonzero unnamed group. The 65 original
zero-scale, zero-palette `null` sentinel descriptors remain reported without invented images.
The object registry, SFT IDs and gameplay behavior are unchanged.

Shared object images belong in `engine/sprites`, with palettes in `engine/textures`. Archive owners
follow the existing explicitly listed original-to-Merge scene layers; arbitrary archives are not
assigned inferred precedence. New outputs remain lowercase/0644, outside both inputs, with rollback
on errors and cancellation. There are now 32 converter stages plus packaging and validation.

Scene sky collection includes original map/delta names, continent settings, map-specific overrides,
existing sky families and the renderer's dawn/night/cloud names. `sky_textures` is a virtual asset
scale category remapped by AssetFileSystem to physical `textures[_xN]`. The importer previously
wrote a physical `sky_textures` directory that the renderer could not read. It now writes the
original images to world-local `textures`; no renderer fallback or generated artwork was added.

## Evidence

| Check | Result |
| --- | --- |
| Global object references | 540 descriptors, 444 groups, 65 sentinel descriptors |
| Exported dynamic art | 963 sprites and 12 palettes; no missing referenced image or palette |
| Independent source comparison | 30,074,266 indexed sprite pixels and all 12 palettes match source |
| World skies | 59 images per world; 177 exports, 31,506,432 RGB pixels match source |
| Previous payloads | All 40,925 prior payloads retained byte-for-byte, allowing sky path relocation |
| Package integrity | All 41,893 final ZIP entries pass CRC; 171 obsolete sky paths removed |
| Map loading | All 201 selected MM6/MM7/MM8 scenes load |
| Focused tests | 18 object/sprite cases and 7 native cancellation/publication cases pass |
| Builds | Final native build 004 and generic unsigned iOS Release 002 pass |

The engine package adds 975 entries. Some relocated skies already existed identically as surface
textures, so world entry counts can decrease without losing an image. Resource verification checks
the original pixels and old payload bytes, not just names or file counts.

`scene-002` renders 181 actual Mac Metal frames from the unchanged normal single-character MM8 save.
First and later captures visibly contain the original cloud sky. The previous `sunsetclouds`,
`arrowa0`–`arrowa4`, `obj187` and `splash1`–`splash6` loading warnings are absent. The scene contains
no active map sprite objects, so warning elimination and correct source pixels do not establish
arrow flight, water impact or every animation in gameplay. English `Menu`/`palm tree` remain.

Failures are retained: the first object conversion mishandled the table loader's normalized null
name; the first full conversion put skies in the unreadable physical directory. Its source-pixel
check passed while actual rendering still failed. After correcting the directory, `navigation-002`
captured directions 0–5 but failed the direction-6 camera assertion. Its cause remains unresolved;
it is not counted as a full navigation pass. The independent `scene-002` success covers loading and
rendering only. No assertion, game rule, save format or production input behavior was weakened.

Evidence: `../local-data/validation/dynamic-presentation-079/`, including `result.json`,
`verify-resources.py`, `resource-verification.json`, original BMPs, lossless PNG copies, tests,
build logs, source hashes and changes. Dummy audio activity is not audible-output acceptance.

## Next milestone

Prioritize a continuous normal MM8 route: create a party, recruit companions, use shops/dialogue,
accept and complete a starting-area quest with dungeon combat/loot, return between maps, save,
restart and continue without altered characters, quest flags or coordinates. Source inspection
has identified Dagger Wound Island's NPCs and dungeon entries; the route is not yet accepted.

Overall remains **40–45%, low confidence**. If this complete starting-area milestone passes without
new foundational blockers, reassess around **55–60%**. Representative progression in all three
worlds, cross-world travel and persistent quest/party state would support a later **70–80%** review.
These are conditional stage estimates, not measured task weights or a promised time schedule.
Complete localization, import/file-provider behavior, physical iPad input/background/audio and long
sessions remain required. Navigation assertion diagnosis and the earlier MM6 candlestick route
also remain open; do not repeatedly rerun already-accepted pixel tests without relevant changes.
