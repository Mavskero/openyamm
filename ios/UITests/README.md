# iPad UI automation

This standalone XCTest project drives the already installed `org.openyamm.ipados`
app. It has no dependency on the game build and does not reset saves, reimport
assets, start a new party or terminate the app. The game must already be running.
The iPad can require its owner to enter the device passcode to enable UI automation.

Build with the same development team used to sign the game:

```sh
xcodebuild build-for-testing \
  -project ios/UITests/OpenYammUITests.xcodeproj -scheme OpenYammUITests \
  -configuration Release -destination "id=$OPENYAMM_TEST_DEVICE" \
  -derivedDataPath build-ios-uitests -allowProvisioningUpdates \
  DEVELOPMENT_TEAM="$OPENYAMM_DEVELOPMENT_TEAM"
```

Set `DEVELOPER_DIR` when using a non-default Xcode installation. Run a JSON plan:

```sh
python3 scripts/run_ipados_ui_plan.py \
  --device "$OPENYAMM_TEST_DEVICE" --plan /absolute/path/steps.json \
  --result /absolute/path/new-result.xcresult
```

Example for the imported Chinese MM6 tavern, already open in landscape:

```json
[
  {"op": "waitText", "text": "孤独骑士"},
  {"op": "tap", "x": 0.89, "y": 0.16, "name": "Select Andover"},
  {"op": "waitText", "text": "安多弗"},
  {"op": "capture", "name": "Dialogue before reward"}
]
```

Coordinates are fractions of the main game window, from its top-left corner.
Available operations are `capture`, `waitText`, `tap`, `doubleTap`, `drag`, `text`,
`key`, `background`, `tapButton`, `tapKey` and `tapCandidate`. The last three use `text` as an
accessibility identifier/label: a native button, software keyboard key, or IME
candidate cell. For example, `tapButton` with `start-game` activates the launcher;
`tapKey` with `c`, `e`, `s`, `h`, `i` followed by `tapCandidate` with `测试` operates
the actual Chinese software keyboard. `tapButton` with `Return` commits the save.
Drag uses `toX`, `toY` and optional `duration`; `waitText` accepts a
timeout up to 30 seconds. Key supports a literal key or `return`, `escape`,
`space`, `tab`, `delete`, `up`, `down`, `left`, `right`.
`background` presses Home, verifies the app entered the background, waits an
optional `duration` (default 2, maximum 60 seconds), then activates the app and
verifies foreground state. It retains both background and return screenshots;
it does not terminate the app or exercise lock-screen/passcode behavior.
Gestures remain limited to five seconds; the complete plan has a 180-second limit.

Plans should check the expected scene before sending coordinates. Every completed
step retains a screenshot in the result bundle; OCR checks preserve recognized
text. Failed scene checks stop the remaining actions. OCR uses Apple's Vision
framework with Chinese and English. It is an observation of rendered pixels,
not an accessibility label or a direct gameplay state check.
Vision receives the UIImage's explicit orientation with its CGImage because
iPad screenshots can store portrait pixels with landscape presentation. Small
bitmap HUD counters can still fail OCR; retain failures and use separately
identified screenshot/save review rather than calling a failed assertion passed.

Export attachments with:

```sh
xcrun xcresulttool export attachments --path /absolute/path/new-result.xcresult \
  --output-path /absolute/path/attachments
```

A successful action plan only establishes its specified UI checks. Copy actual
game saves from the device and inspect them with `openyamm_save_inspect` to verify
rewards and persistence. Synthetic key events do not replace physical accessory,
pointer confinement, perceived audio or long-session testing. Chinese `typeText`
does not establish a real user's IME composition behavior.

Physical status (2026-09-10): the owner enabled UI automation on the M5 iPad.
Full-screen capture/OCR, touch NPC selection, initial letter reward, immediate
repeat, dialog exits, Menu/save-list entry and a new default-name save all pass.
The actual third save contains 11,000 gold, QBit 1106 replacing 1105, and exactly
one retained letter 2125. The first two user saves remain byte-identical.

Use `XCUIScreen.main` for screenshots and the main window for coordinates: the
application proxy initially produced an incorrectly cropped portrait screenshot
despite the actual window remaining 1210x834 points. XCTest MP4 attachments work
on this device even though standalone CoreDevice screen recording was rejected.

Earlier custom name input failed: after a synthetic double tap XCTest reported
no keyboard-focused descendant and refused `typeText`. Synthesized Escape did
not visibly leave the NPC dialog, despite the event API returning. Do not count
either as successful gameplay input in that run. The subsequent save used the normal map-name
default and touch save button. Drag has not been tested yet. OCR also misread a
long Chinese dialogue and missed the italic Menu label; inspect screenshots and
use reliable scene labels plus actual logs/saves for semantic checks.

Evidence: `../../../local-data/validation/ipad-gameplay-031/tavern-quest/`,
including `result.json`, `quest-comparison.json`, plans, results and recordings.
API reference: [Apple UI automation](https://developer.apple.com/documentation/xcuiautomation).

Physical follow-up 032 (2026-09-10): shared touch buffering now preserves two
completed taps received in one event batch and delivers the previous release
before a new held touch. Fast double tap enters the real save editor on iPad.
Direct `typeText` still fails because SDL's UITextField is hidden from the
accessibility hierarchy. Tapping software keys and the Chinese IME candidate
instead saves `测试`; after an app restart it loads successfully. This tests real
IME composition/selection, not direct text injection, but only the exercised inputs.

The keyboard initially hid the edited row. The game now supplies its authored
save-row bounds to SDL in window coordinates. A second installed build keeps
`验证` visible above the keyboard and restores the game frame after Return.
Actual save4/save5 readback confirms names, 11000 gold, QBits and the other listed
fields; previous saves remain byte-identical. See
`../../../local-data/validation/ipad-input-032/result.json` and screenshots.
The automatic entrance-touch probe did not enter the house even though title OCR
passed: the same title was already in the world hover label. Do not accept a scene
transition based on a nonexclusive label or on the runner's success alone.

The owner subsequently entered with E. Automatic letter-topic replay after reload
shows the already-paid Chinese answer and 11000 gold. Save6 readback confirms
unchanged quest bits and the other compared fields; earlier five files are intact.
The same run appended `测试` to `测试版` and tapped software Delete twice: the actual
save name is `测试版`, so deletion of these UTF-8 characters passes as well.

The final 032 build also applies same-frame committed IME text before Return
validation/saving (native failing-then-passing regression). On device, a duplicate
`测试` displays `Save name already exists.` and creates no new file; OCR misread the
bitmap error text, and that failed result remains intact. Visual inspection plus
file comparison establish rejection. Correcting the name to `输入` successfully
saves slot7 with prior files unchanged. See `duplicate-comparison.json` and
`corrected-comparison.json` in the 032 evidence directory.
