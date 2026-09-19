# Emerald Island red potion crafting and delivery — 099

2026-09-12. Normal purchase, backpack crafting, return, delivery and independent restart pass in
the Mac application. The scavenger count is now **2/6: shell and red potion**. The contest and full
MM7 starter remain incomplete. Overall stays **45–50%, low confidence**. No iPad or simulator operated.

## Original stock and normal crafting

The input is unchanged098/shell-route-002/mm7-shell-complete.oysav: normal level-one 中文骑士,
500XP,100gold, five food,35HP and the original seven items. The shell is already delivered.
The Knight has no trained Alchemy skill. Original reagent210 makes potion224, not the requested222.

The actual party walks to original alchemist116, model67/face62/event8, approaching its north-facing
door near(6120,7680,417). Neither shelf in the first observation offered red potion222. Original
standard stock does contain reagent1764 (柳条浆果) for8gold. The imported reagent table maps1764→222.
The final run buys one through an opaque pixel on the rendered shelf; standard slot1 becomes empty
and gold100→92. All other stock entries remain unchanged.

Normal portrait double-click opens the backpack. A left click picks up the purchased reagent; a
right click applies it to the original empty bottle220 through the shared inventory input/item service.
The reagent is consumed and the bottle becomes one identified red potion222 with power1. No character
skill, item, gold, quest bit or position is injected. The basic reagent/bottle rule accepts an untrained
character; this is not coverage of higher potion combinations or mastery failure behavior.

The same session closes the backpack, walks back to headquarters1163, opens the original door and
selects judge341/topic756. It consumes the red potion, clears513, sets520 and grants500XP. The shell
completion521 remains; longbow, tile, instrument and hat flags515–518 remain outstanding. Repeating
the topic gives no additional reward.

`potion-route-003` exits0 with **1644 Metal frames, nine captures and20725.6 walked units**. Final
7out01 position(16079.8936,8559.30371,129), game minute607.126099,92gold, five food,1000XP,35HP,
no conditions/deaths. Six original items remain: only the bottle was consumed by this task.
This is one continuous route from the prior accepted shell save, not a new continuous MM7 campaign.

## Restart, independent audits and prior-mode regression

`reload-001` independently loads the completed potion save, reopens the headquarters and repeats756.
It exits0 after181Metalframes/four captures, retaining both completed items without another reward.
`verify-potion.py` / `verify-potion-001.log` pass:

- Purchase removes exactly one original1764 stock slot for8gold, with unchanged other stock.
- Picked-up reagent matches the purchase; the original bottle becomes222 and is then surrendered.
  The original recipe table is checked independently. Other six original items retain all properties/positions.
- Twenty-five character fields other than inventory/XP,17static fields, remaining tasks, stock and
  the earlier shell completion remain unchanged. Both main/restart cold/warm restores match34fields except name.
- The final reward is exactly500XP, with92gold unchanged during crafting/delivery and no repeated reward.

Because the two MM7 modes share controls, `shell-reload-regression-001` also repeats the earlier shell
restart using its own unchanged098 input. It exits0 after181frames. `verify-shell-reload.py` passes
34cold/warm fields and29reopened static fields, retaining100gold/500XP and an outstanding potion task.
The full shell purchasing route was not repeated with this capture-tool extension.

Actual ready/delivered PNGs were inspected: red potion in the backpack, Chinese delivery text fitting
its panel and headquarters scene are visible. `Mixed potion` remains English; the brief delivery
portrait effect and prior UI/media gaps are not accepted as complete visual or localization coverage.

## Tool changes and retained attempts

Only `tools/ApplicationSceneCapture.cpp` changes. New modes `--check-mm7-potion` and
`--check-mm7-potion-reload` share the earlier normal route/guide/judge handlers. The main mode reads
`mm7-potion-forward.tsv` and `mm7-potion-return.tsv` beside its new output directory. Both80-point
proposals use the unchanged full-collision party planner from098, then pass actual application movement.
The restart mode needs no route files.

All native builds001–005 pass; final005 contains the accepted capture code. No production code,
runtime package, original game input, iOS build or full unit suite changed. The retained production
iOS result is097/ios-release-001. Runtime085/runtime003/save85 remains current; full runtime hashes last090.
Before documentation,52of53recorded098 source paths match; only the Mac capture tool differs.

Retained route001 stops after finding no ready-made red potion. Route002 buys the correct reagent but
the driver uses left click on the bottle, which normally swaps items. Source inspection confirms that
applying a held item uses right click;004/005 correct the test operation, without changing game rules.
Both failure states remain separate and neither is counted as successful crafting or delivery.
All process handles are terminal/reaped, including90418main,92174restart and32787shell regression.

Evidence: `local-data/validation/mm7-potion-099/result.json` and its source/tool/evidence manifests.
Next continue from099/potion-route-003/mm7-potion-complete.oysav. Four scavenger objects remain:
longbow845, tile1438, instrument1434 and hat1433. Investigate ordinary recruitment/equipment and the
remaining source events, then dungeon/contest/travel progression. Current device acceptance,
cross-world persistence and long sessions remain open.
