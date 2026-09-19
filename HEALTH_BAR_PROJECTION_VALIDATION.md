# Health-bar projection — checkpoint 060 (2026-09-11)

The close-enemy health-bar sizing fix is accepted for the observed Mac outdoor rendering issue.
The overall iPadOS port remains incomplete, estimated at 40–45% with low confidence. No physical
iPad was queried, connected, installed, launched or operated. All work and output remain on NVMeSSD.

## Change and scope

Previously a nearly full-size world-space bar could cover hundreds of pixels when an enemy was
close to the camera. The shared policy now derives the bar's scale from its actual view-space depth
and the renderer's projection Y scale. It caps the frame at 1.5% of viewport height while retaining
existing world dimensions at greater distances. Frame, inner fill, gloss and shadow offsets use
the same scale; the shadow extends below the frame as before. HP ratios and nearest-eight selection
remain. Invalid and behind-near-plane bars are skipped.

Both outdoor and indoor rendering pass their actual projection scale. Their code compiles for
macOS and iOS. Actual indoor visuals are still unaccepted; the shared calculation and compilation
alone do not establish indoor gameplay. The change limits size, not general overlap between bars.

## Accepted evidence

Six policy tests with 286 assertions pass both standalone and through `combat_health_bar_check`.
They cover near/far depth, several vertical fields of view and viewport heights, invalid projection,
empty/full/tiny HP and frame/fill containment. Logs: `unit-tests.log`, `unit-cmake-tests.log`.

Actual 2420×1660 Metal images confirm compact close-enemy bars. Pixel analysis of the reviewed upper
capture region finds the old largest rectangular red fill at 978×79 pixels; the successful new
combat capture's largest fill is 195×16. These are fill measurements, not full-frame sizes or a
claim about all maps, angles and occlusion cases. See `pixel-measurements.json` and actual captures.

The ordinary level-one Sorcerer save from checkpoint 056 again completes the actual application
hostile-turn route: queue includes player and seven enemies, incoming damage occurs, two Fire Bolts
reduce target HP 4 → 1 → 0, mana 30 → 28 → 26, and normal corpse interaction grants 24 experience
and 7 gold (total 207). The character finishes at 5 HP. The application renders 249 frames. No HP,
attributes, position, equipment, quest state or cheat settings were changed to make it pass.
This is one kill among active enemies, not a cleared encounter or full campaign.

A separate process using `openyamm_spell_check ... reload` preserves 5 HP, 26 mana, 24 experience,
207 gold, all 260 actor identity/HP/death/invisibility fields, and one already-consumed corpse.
The projectile screenshot shows partial HP fill and the Chinese action-resolution label. The
corpse screenshot shows compact bars and removal of the turn label after realtime resumes.

The generic unsigned iOS Release build passes with explicit external DerivedData. Nothing was
installed. The checkpoint-059 `runtime-002` package is reused; no full import or duplicate package
was generated, and no original game files were modified.

## Build recovery and unsuccessful attempts

Migration changed absolute paths in generated FetchContent metadata. CMake dependency population
left the generated host shaderc absent; `build-002.log` records the failure. Rebuilding shaderc with
the existing bgfx/bx/bimg sources succeeds. Populated dependency sources are now explicitly pinned
to external paths in the local Mac build cache, with `FETCHCONTENT_FULLY_DISCONNECTED=ON`.
`configure-args.json` and `resume-external-build.py` reproduce that local configuration. This is not
a change to dependency versions; deliberate future upgrades must also refresh this local setting.
The formal app, test target and reload tool then build successfully. The first standalone test
compile omitted bx's mandatory build define; the corrected compile supplies `BX_CONFIG_DEBUG=0`.

The first visual attempt loaded the previous 3-HP save near active enemies. Its first screenshot
shows the corrected bars, but the unattended run subsequently entered another screen and failed
the gameplay assertion. Defeat is plausible; the final HP/screen cause was not instrumented. It is
not counted as a gameplay pass. The later normal-caster combat run is the accepted end-to-end route.

The first reload invocation mistakenly used the older Knight-specific diagnostic, whose fixed
starting-equipment check rejects a Sorcerer. Its assertion was not weakened; the correct existing
Sorcerer diagnostic then passed (`mm6-reload-002`). Both failure logs remain available.

## Remaining work

Indoor battles/visuals, multiple-member combat, other spells/weapons, defeat/revival, active-turn
save semantics and long sessions remain open. Original Ice Bolt graphics and several audio assets
are still missing, as are the previously documented MM8/base resources. User-facing import recovery,
full campaigns and future physical iPad acceptance remain part of the full objective.

Evidence directory: `../local-data/validation/health-bar-projection-060/`; summary: `result.json`.
