# World rule import profile

The native `world_rule_import SOURCE BASE_ASSETS NEW_DESTINATION` converter imports
nine original `Data/Tables` files and validates them against existing map and
house registries. It reads the native imported installation, not Python audit
exports, and executes no original scripts. Input files remain unchanged; output
must be fresh and outside both inputs. Failed publication is removed and output
files use lowercase names and mode 0644.

## Source contracts and verified associations

- `Continent settings.txt`: four records in original one-based row order. Keep
  reputation/follower flags, saturation/softness, death movie and water tokens,
  two death-map positions, ordered skies and loading pictures. The fourth
  between-world setting deliberately has no death destinations. All named death
  maps resolve. No replacement death behavior or image/video assets are created.
- `HW water textures.txt`: six original software-texture / hardware-prefix
  bindings, including two distinct source textures sharing `hwoil`. All
  continent-specific water references resolve. This does not convert animation
  frames or establish their bgfx/Metal rendering behavior.
- `House exits.txt`: seven NPC portrait references, free NPC 31, free topic 750,
  and six signed positions in `sewer.blv`. Source coordinate whitespace is
  accepted as the original numeric reader does. Apply the production house-exit
  logic; eight registered houses receive an extra exit. Dynamic NPC allocation,
  portrait images and actual use of each exit remain unvalidated.
- `House rules.txt`: thirteen ordered sections and 412 source numeric records.
  Preserve every original row, including the two Alchem shops Special records
  for ID 19. `RemoveHouseRulesLimits.lua` writes into arrays by ID: the later
  quality 2 overwrites earlier quality 1. The shared loader now follows this
  behavior, producing 411 effective records. All active section-local IDs are
  contiguous, and transport references resolve. Stock generation, service
  interactions, prices and Arcomage gameplay still require acceptance.
- `Outdoor travels.txt`: 24 origins, 62 nonempty directional routes and 34 empty
  edges. Fifteen origins request straight travel. Keep all sides, durations and
  notes, resolve every map, and apply the production map-boundary registrations.
  The supplied unused sixteenth column is empty; unsupported extensions fail.
- `Overlay.txt`: preserve the source's 96 explicitly inactive `0 / null` records.
  This profile rejects active overlay references until their sprite association
  is supported; it does not create stand-in effects.
- `TownPortalSwitch.txt`: four groups with six entries each. Eighteen entries
  reference maps; the last six are map-zero world-selection controls, including
  three `none` icons. Group source values 300/307/308/309 are switch QBits even
  though the historical engine member is named `topicId`. They are not NPC topic
  references. Preserve coordinates, dimensions/defaults, icons, QBits and source
  descriptions. Do not substitute the older `Town portal.txt` source.
- `Transport index.txt`: twenty original MM8 house IDs and three signed location
  references per row. Zero is a valid location; -1 is absent. This required table
  is preserved, but current merged route application uses the four-reference
  Stables/Boats sections in House rules instead. Do not apply both definitions.
- `Transport locations.txt`: 102 contiguous zero-based locations with original
  destination, seven weekdays, duration, coordinates, direction and required
  QBit. All map names resolve case-insensitively. The production house-rule
  application generates 126 routes, removing repeated references within a house
  as the existing runtime does. Actual schedule selection and travel are pending.

## Shared presentation and remaining behavior

Original `1_TownPortalSwitch.lua` uses the localized `MapStats.Name` for nonzero
map destinations. `GameplayUiRuntime` now follows that rule instead of displaying
the English source description. Map-zero controls retain their authored names,
which also carry current engine world-selection semantics. Actual portal layout,
icons, casting conditions and global control localization remain unaccepted.

Original `2_OutdoorTravels.lua` passes the destination side to `WalkToMap` for all
routes. Current shared navigation consumes that side for straight travel, while
other routes fall back to a map start. Registration tests therefore do not prove
arrival-side parity. This difference remains explicitly recorded and must be
resolved before accepting cross-map gameplay. Death/new-game positions, weather,
reputation and follower behavior also require gameplay validation beyond typed
table comparison.

## Offline checkpoint, 2026-09-09

`world-rule-tables-001` contains all nine tables and the conversion report.
`validation/world-rule-source-comparison.json` independently compares every source
cell, verifies 411 effective house records and the exact ID 19 replacement, and
records the applied route/exit counts. Twenty-three focused tests and the complete
427-test resource suite pass. Mac game/native converters and iPadOS arm64 builds
pass. No connected iPad was needed or updated.

`gameplay-base-023` combines these assets with the previous checkpoint. Formal
startup passes the merged world-rule stage and next requires
`engine/data_tables/arcomage_cards.txt`. Optional `house_animations.txt` is still
absent. Output remains `runtime_ready=false`; no complete gameplay loop has passed.
