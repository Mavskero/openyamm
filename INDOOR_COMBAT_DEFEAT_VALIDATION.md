# Indoor attack input and defeat recovery — checkpoint 061

The shared Attack action now accepts a press released between two frames. The existing input
collector retained that press, but world interaction read only `held` and discarded it. Reading
`isActionActive(Attack)` preserves taps and the existing hold/repeat policy. This is shared gameplay
code for both indoor and outdoor scenes; no alternate iPad combat implementation was added.

The overall port remains approximately 40–45% complete, with low confidence. This checkpoint does
not establish a won indoor encounter or complete defeat parity with the original game. All builds,
captures and evidence are on NVMeSSD. No physical iPad was queried, connected, installed or operated.

## Input regression and actual indoor combat

The accepted regression uses a fresh gameplay session, an explicit keyboard Attack binding and
an interaction-ready test world. The same complete SDL key-down/key-up pair reaches the actual
input collector and world interaction controller. The old `held` read fails in `input-before-003.log`;
the final implementation passes in `input-after-004.log`. The check also covers no replay on the
next empty frame, cancellation while blocked, hold debounce and inspect pause. Existing touch,
movement/rest/combat, menu and Chinese save-name checks continue to pass.

The ordinary level-one Chinese Knight save from checkpoint 046 starts inside the Abandoned Temple
(`6d02.blv`), with 35 HP and 200 gold. Its entry originated from the original entrance event, not
a complete walk from town. The capture equips the sword and leather armor already in that save
through the normal equipment policy. It grants no items or attributes, changes no HP or monster
state, and leaves cheats disabled. Camera aiming is diagnostic; movement and Attack are complete
SDL taps delivered through `GameApplication::handleSdlEvent` and normal application frames.

The baseline renders 181 actual Metal frames. In the combat run, the Knight approaches actor 128
from approximately 1,932 to 176 world units and receives actual enemy damage. An actual sword hit
reduces the Great Bat from 21 to 13 HP, with the visible eight-damage message. The solo Knight
subsequently becomes unactable: final HP 0, condition bitset 8320. No kill, experience or corpse-loot
success is claimed. The failed encounter's actual save is retained for the recovery checks.

Reviewed images show temple geometry, enemies, an indoor minimap and compact visible health bars.
They do not establish all close-view occlusion cases. Missing `obj187`/`obj036` sprites and combat
sound files remain reported by the loader. The hit message still mixes Chinese names with English
text; full combat-message localization remains open.

## Actual defeat and cold reload

The input is `temple-combat-002/indoor-failed.oysav`. The diagnostic only observes the normal
application defeat path; it does not call consequence/respawn methods or modify the loaded party.

- Explicit skip: original `6losegame` video appears, normal Escape input at frame 120 closes it,
  and recovery occurs at frame 121. The application renders 302 frames, then saves.
- Natural completion: no skip input is sent. The video advances and finishes; recovery occurs at
  frame 888. The application renders 1,069 frames, then saves. Background music is no longer paused.
- Recovery returns to `oute3.odm`, matching the original Continent settings row for Enroth:
  X −9728, Y −11319, direction 512 (π/2). Foot Z settles at 161 above the configured 160 floor spawn.
  HP becomes 35/35, conditions clear, gold changes 200 → 0, and the death counter changes 0 → 1.
  More than 180 subsequent application frames do not apply another penalty.
- A separate process loads the recovered save through the actual application, renders 181 frames
  and saves again. Gold remains 0, HP 35 and deaths 1. No extra recovery screen appears.

`compare_defeat.py` reads all four real saves with `openyamm_save_inspect`. It verifies character
progression, skills, equipment, inventory, quest bits, food and the saved indoor actor records.
All 223 actors preserve identity, HP, position, flags and group; the hit bat remains at 13 HP.
The save carries both a legacy filename key and a canonical world/map key for that same dungeon;
these are not 446 different actors. The bank was zero, so this does not test a nonzero deposit.
The Knight has no mana, so this run is not a nonzero-mana revival test. Original elapsed-time,
follower, special-map and other-world defeat effects remain outside this acceptance.

The video screenshots are real Metal output. SDL dummy audio is used for unattended Mac capture;
audio cache/background-pause assertions do not establish audible playback or physical iPad behavior.

## Diagnostics corrected during the work

The first indoor capture mistakenly sent a keyboard scancode for the default mouse-bound Attack
action. It therefore cannot show a production attack failure. Later captures explicitly bind F.
Early regression attempts likewise had a mouse binding or a test world whose inspect mode bypassed
the relevant interaction branch. They are retained but excluded from accepted before/after evidence.
Only `input-before-003.log` and `input-after-004.log` form the accepted regression pair.

The first external save comparison incorrectly expected negative HP; the actual failed save is at
zero HP. The runtime check independently requires no actable member. The inspector was also fixed
to avoid labeling a retained indoor snapshot with the current outdoor map name after respawn.
The corrected comparison passes without modifying game saves or relaxing gameplay requirements.

## Build, evidence and next work

The Mac capture, input and save-inspection targets build successfully. The production input fix
passes a generic unsigned iOS Release build (`ios-release.log`), with explicit external DerivedData.
Later changes are confined to validation tools and records. No full resource conversion was needed;
checkpoint 059's `runtime-002` is reused.

Evidence root: `../local-data/validation/indoor-combat-061/`; machine-readable summary: `result.json`.
The capture CLI adds mutually exclusive `--check-indoor-combat`, `--check-defeat` (skip) and
`--check-defeat-complete` (natural finish), each taking runtime, input save and a new output directory.

Next: a won indoor encounter using a normally created party; active-turn save semantics; other
weapons/spells, full original defeat rules, MM8/base missing resources, full tasks/travel, import
recovery and sustained stability. Physical iPad acceptance stays deferred under the user's instruction.
