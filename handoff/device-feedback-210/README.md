# Integrate receiving iPad fixes 210

Merge receiving branch `dcae3d5608060cf7dc4e359f77f11f8a098ad28e` into the current `codex/ipados` work. Its production fix is `7aba6c0cc4bdf36e63e75c9f75da6733d9877c0c`, based on the fixed release 195 source. The only textual conflict was equivalent comments about retaining a failed-save name draft; current wording and behavior are retained.

Included fixes expose touch inspection during creation and in the Standard character HUD, initialize empty save names from occupancy rather than translated labels, and provide 100 manual slots with correct scrolling, absolute indices and IME placement. Later main-branch UI, text, settings and input fixes remain present.

The [receiving report](../ipados-device-195-20261029/README.md) reports ordinary carriage travel, Chinese Save104 saving, Home resume, independent-process load and speaker audio passing on its final patched app. Ten old originals remain unchanged. Its raw evidence remains on the receiving Mac; this merge does not independently inspect those pixels or rerun a device flow.

Long NPC/questions, targeted scrolls, sleeping-member potions, bank and shop conditions, inventory detail, key rebinding and natural failed travel remain unverified on that device. Complete natural campaign completion remains unproved. Current engineering estimate is approximately 85%, near the upper end of the prior 80–85% range; it is not measured quest coverage.

Integration validation is restricted to native compilation, the existing bounded input check, and unsigned generic iOS compilation. No new cases, App/campaign replay or current-host device/simulator operations. Complete runtime207 and Save104 remain unchanged; release195 stays fixed.
