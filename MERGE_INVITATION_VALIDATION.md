# Merge invitation — checkpoint130 accepted

2026-09-24, single main Astra. Evidence is in
`../local-data/validation/merge-invitation-130/result.json`.

The original SHA-gated invitation subset now generates770/771/772 from ordinary chest opening when
Party[0].Experience is strictly greater than50000. The shared chest runtime preserves the first empty
logical item record, including cached chests. Full chests retain the original generation-flag behavior.
Successful ordinary pickup invokes GotItem. Natural Dimension Door activation completes its existing
timer before opening Verdant through the existing NPC entry and dialogue flow. Item190 remains separate.

The actual Mac run starts from the unchanged accepted127 save. A declared fixture changes only the first
member's experience7611→50001. It walks to original chest1, opens it, generates772, takes it with a pointer,
stores it through the portrait, reopens the chest, then walks to original fountain event140. Greeting329
and the four introduction actions1778/1779/1779/1780 complete the original flags with zero experience reward.
The final introduction sets greeting328 and removes its topic. Long Chinese paragraphs now rewrap at a
smaller font scale when the existing small font exceeds the available dialogue body height.

`actual-004` and a separate process `reload-002` both exit normally. The reload preserves every inspected
field except the save title at both cold and warm restoration, then walks back to the chest and fountain
without regenerating the letter or repeating the introduction. The213 independent save checks pass.
Final resources remain9067 gold/14 food, no deaths, HP33/22/21/22; exactly one772 remains in inventory.
Nine key screenshots were reviewed, including all four complete Chinese introduction bodies.

Validation:

- Invitation/chest/save coverage:25 cases,654 assertions.
- Existing Town Portal contact:7 cases,488 assertions; Dimension Door:26 cases,519 assertions.
- Existing introduction:4 cases,273 assertions; chest opening:2 cases,8 assertions.
- Dialogue layout perimeter:3 cases,27 assertions; actual long-text render reviewed separately.
- Fresh scoped event import passes the original source SHA gate; generated content matches the candidate.
- Save format86 adds chest record identity/capacity. Three original85 normal saves retain all44 inspected
  fields on roundtrip. Previously cached85 chests cannot recover discarded original indices; their explicit
  normalization uses retained visible/hidden item order when an insertion first needs record identity.

The complete runtime is `runtime-001`: two changed engine members and the unchanged three129 world ZIPs.
The original source, normal saves, earlier failed runs, and diagnostic history remain intact.
`actual-003` passed functionally but was not accepted because two Chinese paragraphs overflowed;004 fixes it.

This is a real chain under a controlled experience prerequisite, not naturally earned campaign progress.
It does not establish the full Merge main quest, late rewards, lost-item recovery, iPad controls, device
performance, or subjective audio quality. Endurance131 and fresh import/final build132 follow separately.
