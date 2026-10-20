# Mac endurance — checkpoint 131

2026-09-24. Single main Astra (`gpt-6-astra`; reasoning effort not exposed).
Evidence: `../local-data/validation/mac-endurance-131/result.json`.

A normal MM7 cleared-castle continuation ran through actual Mac Metal for **30 minutes** in one process:
105,821 frames, 25.61 minutes of measured physical movement, ten ordinary castle exit/re-entry roundtrips,
29 inventory/character/skills/journal/menu cycles and 70 saves. Ordinary input and collision determine movement;
no coordinate/stat/experience fixture, immortality or unlimited mana was used. The four accepted130 ZIPs and
accepted128 history-supplement input remained byte-identical throughout the run.

`actual-001` and independent `reload-001` exited normally. The 680-check save audit preserves all four characters,
HP 45/34/24/36, SP 0/37/31/7, conditions zero, gold128, food14, deaths zero, quests, inventory and cleared indoor
actors/chests/mechanisms. Cold and warm restoration match all 44 inspected fields except the save title.
Normal time/position/selected character and outdoor NPC simulation advance; they are recorded separately.

The route visits original inn240 physically. Two captures differ inside the video area, the decoder reports460x344,
and closing the house releases its active frame and resumes background music. SDL dummy audio proves decoding
and playback state only; subjective sound quality and audiovisual synchronization were not evaluated.

62 periodic/final samples show median interval p95=17ms. The largest frame interval is2166ms in a map-transition
interval. RSS peaks at1,694,793,728 bytes initially and ends at175,521,792 bytes; after the inn, sampled RSS ranges
95,207,424–175,816,704 bytes. Audio stabilizes at77 cached clips and87,214,176 PCM bytes during the final outdoor
segment. RSS can fall through reclaim/compression and is not an allocation/physical-footprint measurement.
No monotonic resource growth was observed in this bounded run; it does not prove unlimited-duration stability.

Visual review found a preexisting journal defect: the selected original MM6 interface uses1x1 corner decorations,
but the renderer enlarged them to the default MM8 decoration dimensions. The shared journal renderer now uses
those three selected textures' logical native dimensions. No asset or color-key workaround was introduced.
After build007, `pilot-006` exercised all three journal pages, a castle roundtrip, house video and five UI cycles
for120.081 seconds. It exited0;257 state checks passed, and the three actual page captures show the block removed.
The formal run's original failed visual is retained. This isolated rendering fix was checked through the affected
fragment under handoff131 section6; the earlier full run remains identified by its frozen source/binary hashes.

The initial package still lacks dynamically referenced loading1–5/loadprog/bardata art, also absent in earlier
accepted packages. This known loading-presentation gap is explicitly queued for132's original-source reimport;
it is not suppressed or presented as a complete-resource pass. Current evidence does not cover iPad performance,
input/lifecycle, arbitrary maps, full natural campaigns, all UI translation, or subjective media quality.

Original pilot001–004 and build001 failures remain in the failure ledger. The user’s broader goal remains active;
checkpoint132 will perform fresh full import and final native/unsigned generic iOS Release integration.
Overall engineering estimate remains55–60%, low confidence; checkpoint counts are not campaign coverage.
