# Turn-based queues and dialogue readability — checkpoint 058 (2026-09-11)

All work in this checkpoint is on Mac or a generic unsigned iOS build. The iPad is not queried,
connected, installed, launched or operated. Overall progress remains a low-confidence 40%–45%.

## Shared turn queue fixes

Removed the unconditional exclusion of actor index 10. Queued actors are now removed when they
are dead, unavailable, no longer hostile, or beyond the same nearby-distance limit used at entry.
Before each attack phase, newly nearby hostile actors are added without duplicating existing
entries or resetting their initiative. Party readiness/recovery rules and the existing wait/attack/
movement phases are retained. This updates the shared runtime for indoor and outdoor worlds.

A focused target runs the 12 existing turn-runtime tests and three new regression cases. Before
the fix, the new cases fail with eight assertions (the skipped index, four invalidation states,
and new-actor discovery). Afterward all 15 cases / 150 assertions pass. These include the existing
pending-action, recovery, movement-budget and once-per-turn time checks. These are logic fixtures,
not proof of an entire combat encounter. The repository's tracked doctest/common-script folders
were added to the sparse checkout so normal builds can use those existing dependencies.

## Actual Mac application flow

The ordinary saved single level-one Sorcerers from 057 are loaded through GameApplication with
no preseed party, immortality or unlimited mana. Shared GameplayInputFrame actions enter turn
mode, reject forward motion in the attack phase, cast learned Torch Light at its normal one-mana
cost, pass remaining actions, hold the movement phase idle for 30 frames, consume five movement
steps (130 → 104 → 78 → 52 → 26 → 0), reach another player turn, and return to realtime.
The actual collision-aware displacement is about 780 units in both starting scenes.

Both starting positions initially queue only the character: nearby enemies are absent. Thus these
runs prove the controls/state cycle and spell buff, not hostile attack resolution or a turn-based
kill. Actual near-hostile queues, projectiles, full battles, indoor turn combat and active-turn save
semantics still need integration checks. These saves are made after returning to realtime.

MM7's original floor event 200 opens the Chinese guide dialogue after the fifth movement step.
The diagnostic initially timed out because it did not close that modal. The accepted sequence
verifies that the wait phase stays paused for 20 frames, sends the shared Escape input, and reaches
the next player turn. It neither disables the event nor edits quest state to skip the guide.
MM6's first diagnostic also incorrectly checked the session's stored time instead of the active
world's clock; the check was corrected, without changing production time rules. Failed runs remain
in the evidence directory and are not counted as accepted. Separate-process reloads preserve party
health/mana, learned spells, consumed book and all saved actor HP for the accepted pre-dialogue-fix
runs (`mm6-controls-002`, `mm7-controls-003`).

## Dialogue change requested during this checkpoint

The user reported that dialogue text was too large and crowded. The authored DialogueText scale
is reduced to 0.9; attribute/skill-page and topic/title settings are unchanged. The body renderer
now honors that scale. It computes Chinese wrapping and line height from the same CoreText size
used for drawing, including descent, leading, raster padding and shadow, with a small interline
gap. Frame sizing and drawing use the same wrapped-line metrics rather than the legacy bitmap
font height minus three. Text is not shortened or rewritten to make it fit.

Final resource/capture validation is recorded in result.json. Screenshots from the earlier runs
expose the original cramped Chinese guide text. The original turn-indicator names turnstop,
turnhour and turn0–turn4 have no matching texture names in the current packages; visual turn
phase indicators are not accepted by the behavioral checks. Missing common spell/footstep/turn
samples and character portraits remain separate resource gaps.

Evidence root: `../local-data/validation/turn-combat-058`. Tests are `tests-before.log` and
`tests-final.log`; the final build and accepted captures are listed in `result.json`.

The final native import loads all 140 maps; 27,527 ZIP entries pass CRC and byte comparison.
Only the authored dialogue.yml differs from 057. All 23 UI-import checks pass. The final Mac
`mm7-dialogue-001` run renders 181 frames, shows the complete original guide paragraph in four
separated lines, closes the dialogue, reaches the next turn and saves. Its actual Metal screenshot
was inspected; the crowded-line defect is resolved at 2420×1660. Other dialogue lengths and
viewport sizes still need broader visual review. `ios-release-final.log` reports BUILD SUCCEEDED;
the unsigned generic Release is not installed. Final later edits only wrap two long source lines.
