# UI continuity fixes 197

Development follow-up to fixed remote release 195 (`18cd05fd`). The published device package remains unchanged.

## Changes

- Notes uses the existing cross-map acquired-note query, so notes collected on another map remain visible.
- Skill presses retain the party member identity. Cycling characters cancels the pending gesture before release.
- Awards rendering and pointer input share wrapped row heights, scroll range and dynamic thumb geometry. Mouse/touch dragging updates the same offset and cancels on identity change.
- Manual saves distinguish a real slot from its localized empty label. A failed asynchronous save keeps the entered name and existing slot metadata for retry; a saved custom name `Empty` stays valid.
- A new journal/zoom key press is accepted even when it immediately follows the previous frame's press. Save Return/Escape uses the same fresh-press rule.

Runtime 195 and Save104 remain unchanged. No current-host device or simulator operations. No new unit cases or broad campaign replay.

## Validation

Native build passed. The first iOS link used a stale generated project without the new layout source; the failed record is retained. Refreshing CMake and rebuilding unsigned generic iOS Release passed.

The first actual Mac flow passed in 14.095 seconds / 77 frames, including 30 normal continuation frames, a real failed save and successful retry, and final save/decode. All three screenshots were reviewed. See `scoped-result.json` and `actual-fixture.txt` for the controlled note, skill, awards and SDL held-input prerequisites. This is scoped UI evidence, not natural campaign or target-iPad acceptance.

When adding C++ sources, refresh the generated iOS CMake project before invoking its Xcode app scheme.

The receiving thread reported that release195 was installed without erasing user data, all six resource files passed device readback verification, and ten old saves were backed up. It is waiting for a user-local system authentication step for UI Automation; no new in-game device acceptance has been reported. Raw receiving-host evidence has not been independently inspected here.
