# Hostile turn combat and visible phases — checkpoint 059 (2026-09-11)

This checkpoint uses Mac application/headless checks and a generic unsigned iOS Release build.
No iPad query, connection, installation, launch or operation occurs. Overall progress remains
a low-confidence 40%–45%; the new encounter evidence does not establish complete campaigns.

## Normal hostile encounter

The application capture tool adds an independent `--check-hostile-turns` route. It loads the
ordinary saved level-one Sorcerer from checkpoint 056, already near an original MM6 enemy after
normal movement and one earlier realtime Fire Bolt hit. Initial health is 16, mana 30, experience
0 and gold 200; the target starts at 4 HP. No character attributes, actor HP, equipment, position,
quest state or cheat settings are changed to make this encounter pass.

Shared input frames enter turn mode, pass actions and move toward the corpse. The diagnostic
aims the actual camera, arms Fire Bolt through the scene adapter, and resolves its original target
through the shared spell service. It is explicit diagnostic input, not physical keyboard/mouse
validation. Projectile flight must block further player action until impact. Actual incoming
damage, normal mana cost, target HP loss, death, experience and removal from the turn queue are
asserted. Collision-aware movement approaches the corpse before leaving turn mode; shared corpse
looting must not duplicate gold/items on a repeated interaction.

Two Mac Metal encounters pass. The final `mm6-combat-002` run uses the current package and HUD:
the initial queue contains the character and seven hostile actors; two casts reduce target HP
4 → 1 → 0 and mana 30 → 28 → 26. The character survives with 3 health and gains 24 experience;
corpse gold is 6, bringing the total to 206. The application renders 250 frames and saves after
returning to realtime. The earlier run also passes, with randomized damage/loot resulting in
6 health and 207 gold. This proves one enemy kill amid active hostiles, not clearing the encounter.

The cold-reload diagnostic now additionally compares experience, gold, food, actor identity,
death/invisibility state and consumed corpse state. Consumed corpses must stay unavailable to
loot. Separate-process checks pass for both encounter saves; the final `mm6-reload-003` preserves
3 health, 26 mana, 24 experience, 206 gold, all 260 actors and one consumed corpse.

## Visible phases and dialogue regression

The shared gameplay HUD adds an authored noninteractive Chinese text panel at the top center.
It displays wait, character action, pending action resolution and remaining movement steps,
then disappears when turn mode ends or another HUD screen opens. It uses the existing solid-color
UI and text renderer; no original icon or unrelated image is substituted. The old animated turn
icon source gaps remain documented. Both indoor and outdoor gameplay use this shared renderer;
indoor visual behavior still needs an actual capture.

Actual 2420×1660 Metal captures show readable wait, pending-resolution and five-step labels,
with no overlap with the gold/food bars. The realtime corpse capture confirms the label is gone
and gold is 206. The final MM7 control regression renders 181 frames, consumes five normal steps,
opens the original guide dialogue, closes it through shared Escape and reaches another turn.
Its Chinese dialogue paragraph remains complete in four separated lines with checkpoint 058's
0.9 body scale and measured line spacing. Attribute/skill-page sizes remain unchanged.

## Import/build verification and retained failures

Native conversion `runtime-002` passes all 140 converted-map loads. All four ZIPs pass CRC;
27,527 entries have the same names and bytes as 058 except `engine.zip/ui/gameplay/gameplay.yml`.
All 23 UI-import checks and the generic unsigned iOS Release build pass. Nothing is installed.

The initial native assembly failed with an iostream error while free disk space was low; the
message does not identify an ENOSPC errno, so disk exhaustion is a suspected cause, not a proven
diagnosis. Its incomplete output was rolled back and the failure log retained. Byte-identical
world archives from earlier validation checkpoints were verified by SHA-256 and replaced
atomically with independent APFS copy-on-write clones. Paths, bytes and future independent writes
are preserved; user source files and earlier evidence remain. Conversion then succeeds. The
final package's identical world archives receive the same storage optimization after checks.

The first HUD build failed because the turn-runtime type was incomplete; adding its direct
header fixes the build. An initial reload ran the prior diagnostic binary and is preliminary;
`mm6-reload-002` and `mm6-reload-003` contain the extended assertions and are the accepted checks.

## Remaining limits

- Enemy Ice Bolt sprite names `sp26b10` through `sp26b80` are absent from all 41 original LOD
  indices. MM7-prefixed variants exist, but their equivalence to the missing shared source is
  unestablished; they are not silently reassigned. Damage works, but that visual effect is not
  accepted. Common spell/turn/footstep samples and default character portraits remain missing.
- Close enemies produce excessively large overlapping health bars in the corpse capture. This
  visual issue remains open; passing combat assertions does not accept all combat presentation.
- Indoor battles, other spells/weapons, multiple-member parties, defeat/revival, complete fights,
  active-turn save semantics, sustained sessions and full campaigns remain unaccepted here.
- Mac diagnostic frames and dummy audio provide no new physical iPad/accessory/audio acceptance.

Evidence: `../local-data/validation/hostile-turns-059/result.json`, accompanying logs and actual
Metal captures. The original source directory is unchanged.
