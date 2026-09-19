# Spellbook text controls — checkpoint 057 (2026-09-11)

Mac development and generic unsigned iOS builds only. No iPad query, connection, installation or
operation. The previous checkpoint's blank spellbook is now an operable text-button presentation;
the missing original icon artwork and missing common audio remain recorded source gaps. Overall
progress remains an estimated 40%–45%, with low confidence.

## Shared UI change

The authored `engine/ui/gameplay/spellbook.yml` now uses Chinese school tabs, text spell rows and
explicit Cast, Details, Quick Spell, Attack Spell and Return controls over the original page images.
All 12 schools retain their canonical spell/layout IDs. The two-column layout fits each school's full
11-spell or 4-ability range; unlearned spells and unavailable schools do not activate. Names and
descriptions come from the imported spell table; controls use the authored Chinese UI labels.

This is an explicit presentation choice: a layout element with a font and no primary image is drawn
as a text button. A declared-but-missing image does not automatically become another asset. Older
image-button layouts still use their existing path. Solid button fills are engine UI primitives;
no spell artwork is generated or reassigned. Text controls, spell knowledge and binding state remain
shared across worlds, indoor/outdoor scenes and pointer sources.

Single release selects a spell. The Cast button and existing double click use the same normal
scene-adapter casting entry, keeping the book open if the caster cannot act. The Details button
opens the existing inspect overlay for the selected spell without requiring held right mouse input;
a subsequent left/touch press dismisses it and consumes that interaction so the underlying button
does not fire. Escape dismisses that detail state first. Existing held-right-mouse inspection remains.
The Chinese body text displays, but the older inspector's school/mastery/SP labels remain English.

Quick/attack bindings retain their existing toggle semantics. The selected spell's button labels
show clear actions when already bound. Clicking another control now clears the prior spell-row
double-click record; a quick-binding action between two row clicks cannot accidentally turn the
last row click into a cast. This fixes interaction state rather than weakening the double-click
interval or changing spell rules.

## Validation and its limits

- The native full import completes and loads the same 140 maps. The four ZIPs contain 27,527 entries
  and pass CRC checks. Compared with checkpoint 054, only `ui/gameplay/spellbook.yml` changes;
  `gf-aspell.bmp` and `gf-aspelld.bmp`, formerly referenced only by the replaced image control, are no
  longer packaged. Every other resource payload remains byte-identical, including all three world
  packages. The original files are untouched, and no gameplay table or save format changes.
- All 23 existing native UI-import fixture checks pass, including original skin/localization
  precedence and publication failures. They do not by themselves prove interactive gameplay.
- A normal Sorcerer is created through NewGameScreen in each of MM6/MM7 with two legal optional
  choices, Air Magic and Water Magic, alongside default Fire Magic/Staff. Starter knowledge is
  1/12/23 with normal level-one skills; the actual held-item service consumes book 401 to learn
  Fire Bolt. No debug levels, unlimited mana, immortality or manually overwritten spell knowledge.
- In the formal Mac GameApplication, the input controller receives explicit touch-origin and
  mouse-origin GameplayInputFrame press/release sequences. Both worlds pass release-outside
  cancellation, single selection without casting, persistent details and dismissal without
  click-through, quick binding/clearing/rebinding, protection against stale double click, attack
  binding, Fire/Air/Water switching, and unlearned school/spell guards. A text Cast consumes normal
  Torch Light mana; an immediate recovering cast is rejected and keeps the book open. After real
  gameplay recovery, Fire Bolt enters actor-target selection without spending mana prematurely.
  The diagnostic then cancels that target selection and returns to gameplay.
- Both formal runs render 286 Metal frames and save. Actual list and detail images were inspected;
  Chinese names and text fit the tested scene. The runtime resolves 1,404 rectangles across 12
  school layouts and four viewport sizes (640×480, 1210×834, 2420×1660, 1366×1024), with no bounds
  or overlap failure. This covers geometry for every slot, not a visual/playthrough test of every
  learned spell or every description's length.
- Setting a quick spell also queues an original character reaction through the real gameplay audio
  path: sound 5025/pc5013f in MM6 and 5024/pc5013e in MM7. Original music remains active. Common
  click/open/close/turn-page/cast samples still missing from the source are not counted as audible.
- Separate-process reloads preserve all learned spells, consumed book, 31 remaining mana, saved
  actor HP and both quick/attack bindings to 霹雳火焰. Generic unsigned iOS Release builds successfully.

The first control run stopped when its diagnostic tried another spell while the book had paused
recovery from an earlier diagnostic cast. The accepted sequence starts with a ready saved caster,
tests the rejection, returns to the world and waits for actual readiness. No cooldown or game-time
rule was modified to make the check pass. The failed run is retained and is not counted as accepted.

These are automated Mac input-frame checks, not physical touch, keyboard/mouse hardware, iPad
rendering, physical audio or lifetime acceptance. Full spell effects/target interactions, remaining
Chinese shell labels, indoor and turn-based combat, full campaigns and cross-world travel, resource
gaps, import recovery and long sessions remain open. The user’s no-iPad restriction remains in force.

Evidence root: `../local-data/validation/spellbook-controls-057`. Accepted resources are `runtime-001`;
accepted formal runs are `mm6-controls-002` and `mm7-controls-001`; normal startup/learning and binding
reloads have their corresponding world prefixes. See `package-comparison.json`, `ui-import-tests.log`,
`ios-release.log` and `result.json`. `spellbook-before.yml` preserves the previous authored image
references for the missing-source comparison from checkpoint 056.
