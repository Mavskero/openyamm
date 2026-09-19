# Normal caster initialization and spell checks — checkpoint 056 (2026-09-11)

Mac development only. No iPad query, connection, installation, launch or operation. The accepted
checkpoint-054 runtime package remains unchanged. Overall progress stays an estimated 40%–45%,
with low confidence; the spellbook's missing source artwork prevents complete player-facing acceptance.

## New-character correction

The supplied `Scripts/General/MenuChooseCharacter.lua`, `PrepareChar` (around lines 267–340),
grants basic spell knowledge according to each selected magic skill's level, grants racial abilities
according to mastery, and provides a book for the second ordinary spell in each learned school.
The previous OpenYAMM startup cleared all ordinary spell knowledge and supplied both first and second
spellbooks. A normal Sorcerer created through NewGameScreen reproduced that difference before the fix.

The shared new-session loadout now learns the first skill-level-count spells within the school's
range, supplies the second spell's book, and synchronizes innate ability knowledge. At normal starting
level one, Fire Magic means Torch Light (ID 1) learned and Fire Bolt's book (item 401) in inventory.
The redundant Torch Light book (400) is no longer granted. This is new-character initialization;
it does not rewrite existing saves. Other randomized starting items and full class/race parity are
outside this correction. No generated Lua, source data, item/spell IDs or save format is changed.

## Accepted Mac evidence

- `openyamm_creation_check --class=Sorcerer` cycles the real creation-screen class choices, allocates
  legal bonus points, selects two legal optional skills, enters a Chinese name, and starts through
  GameApplication with debug loadouts, immortality and unlimited mana disabled. MM6 and MM7 both
  create a level-one Sorcerer named 中文法师 with 33/33 mana, known spell 1 and one book 401. MM6
  starts in oute3 with the normal letter; MM7 starts in 7out01 with its scavenger-hunt flags.
  The default Knight creation checks for all three continent selections and a full MM6 Knight start
  continue to pass. This does not load the MM8 geography or cover every caster/race combination.
- `openyamm_spell_check` loads these actual saves into GameApplication. The real shared held-item
  service learns spell 2 and consumes exactly one starter book. Shared spell resolution applies
  Torch Light, deducts one mana and imposes recovery; an immediate repeat is rejected. Unlearned
  Fire Bolt is rejected before learning. Repeated normal casts with elapsed recovery spend the
  remaining 32 mana, and casting at zero is rejected, separately in both worlds.
- A normal MM6 movement/AI/projectile run approaches an original hostile actor, requests Fire Bolt
  targeting, selects that actor and advances its actual projectile. Accepted run `mm6-fight-003`
  damages actor 226 (低级魔法师) from 6 to 4 HP and spends 2 mana, 32 to 30. No enemy HP, party
  damage, level, mana or position is overwritten for this check. This is one impact, not a completed
  spell-only battle or turn-based-combat acceptance.
- Three separate-process reloads preserve learned spells, consumed book, health, mana, Torch Light
  and every saved actor HP: the MM6 impact save and the MM6/MM7 zero-mana saves. They write new saves.
- Formal Mac GameApplication runs load the learned-spell saves in both worlds, use the actual
  view-backed cast entry point, verify Torch Light's buff, cost (32 to 31) and immediate-repeat
  rejection, then open the real spellbook. Each renders 181 Metal frames and saves. These calls are
  diagnostic actions, not physical touch or keyboard/mouse input. Original music 37/35 starts.
- Generic unsigned iOS Release builds successfully. No device installation follows.

The early headless diagnostic attempted the view-backed cast entry without a scene view adapter,
causing a null-adapter crash. The final headless tool uses the shared resolver; formal Metal runs
cover the adapter path independently. An initial combined MM6 fight/mana-exhaustion check left the
caster casting harmless buffs beside enemies and eventually failed readiness; the accepted fight
check ends after its impact save, while exhaustion is tested independently at the normal entrance.
These unsuccessful diagnostic runs remain recorded and are not counted as passes.

## Observed visual/audio gap

Both actual spellbook captures show the original page and Chinese character name but lack the known
spell icons. The 41 source LOD indexes were checked against all 264 distinct image references in
`spellbook.yml`: 14 references exist in the source and all 14 are already packaged; 250 do not exist.
The missing set includes `sbf01a/b`, `sbf04a/b` and `STFd`. This is not a missed import of those exact
names. No alternate artwork was fabricated or unrelated school images substituted.

Formal casting also requests absent `torchlight.wav` (sound 10000), and opening the book requests
absent `openbook.wav` (163). Prefixed original-world variants exist, but were not silently assigned to
the common IDs. These are part of the already recorded source-audio gaps, not audio-playback passes.

Player-facing selection/casting, school switching, quick-spell binding and inspect text still need
work. In particular, the missing icons must not be mistaken for a functioning spellbook simply because
the underlying rules pass. Other spells, indoor effects, healing/statuses, turn-based combat, full
campaigns/travel, missing MM8 resources, import recovery and later physical acceptance remain open.

All evidence is under `../local-data/validation/normal-spells-056`: `mm6-before.log`, accepted
`mm6-start-001`/`mm7-start-001`, `mm6-knight-001`, `mm6-fight-003`, `mm6-learn-003`,
`mm7-spells-002`, the three `*-reload-001` runs, `mm6-metal-001`/`mm7-metal-001`,
`spellbook-source-audit.json`, `ios-release.log` and `result.json`. BMP/PNG captures are actual
Mac-rendered pixels. Runtime resources remain `sound-payloads-054/runtime-001`.
