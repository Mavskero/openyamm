# Interrupted resource imports — checkpoint 069

The native Foundation importer now recovers interrupted copies and conversions before new operations.
The launcher runs recovery on a worker queue while its actions are disabled, then restores the selected
resource and reports when incomplete data was removed. This iOS interface was compiled, not exercised
on an iPad or simulator. All development and evidence remain on NVMeSSD; no physical iPad operations.

## Storage and publication

`GameImports/.operation.lock` is held with exclusive nonblocking `flock` throughout copying, conversion,
publication and recovery. A competing operation returns an error instead of deleting live output.
The lock file stays in place; closing the descriptor or process death releases ownership. This does
not use PID files or assume that an old timestamp means a worker has stopped.

Before new output is created, `.pending-UUID.json` records schema 1, the exact UUID and operation
`copy` or `convert`. Both operations write into `.staging-UUID`. Success renames staging to `UUID`, then
atomically replaces `active.json`. The pending journal is removed afterward. Normal failure removes
only this operation's new output; if removal fails, its journal remains for recovery. Active import
records retain their existing schema 1; resource ZIP contents and save version 85 do not change.

Recovery handles interrupted staging and the rename-before-publication gap. A published active UUID
and its source UUID are protected, with UUID case normalized. Unrecognized/future journals and symbolic
links are preserved. If the active selection is unreadable, a journaled final directory is retained
because publication cannot be established. Recognized legacy `.staging-UUID` copies without a journal
can be removed. Unmarked UUID directories may be completed older imports or incomplete old conversions;
the recovery code does not guess or remove them. No completed-import history cleanup is implemented.

The CLI `apple_import --recover SUPPORT_DIRECTORY` calls the same implementation used by the launcher.
The C++ conversion exception is explicitly decoded from UTF-8 for the Chinese error message. Using `%s`
previously dropped the reason in the native regression's error output.

## Native verification

`tests/test_apple_resource_import.py` has 20 passing cases in `tests-003.log`. Coverage includes the
previous source/runtime copy, byte preservation, executable filtering, symlink and conversion-failure
checks, plus recovery boundaries, active/source protection, corrupt/future metadata, competing locks,
publication failure and a real killed copy process. The latter observes partial files, confirms the
child is stopped, proves recovery is refused while its lock is live, sends SIGKILL, waits for terminal
status, then verifies cleanup and identical active-selection bytes. These use owned test fixtures.

The supplied real source was also imported with the native API. All 377 copied files (1,303,024,352 bytes)
hash-match their original files. A real converter was stopped at the story stage with 24 partial files;
recovery was refused while it was alive. After confirmed SIGKILL, recovery removed the incomplete
conversion and preserved the active source. A new complete conversion then passed all 140 map loads,
published a new runtime ID and left no pending/staging entries. All 377 source and copy hashes remained
unchanged, and all four resulting ZIP hashes equal the already-validated 063 package exactly.

A separate actual Mac Metal application then loaded the normal 068 castle-delivery save using this
published runtime directory, rendered 181 frames and saved. The compared map, party, inventory, gold,
experience, quests and NPC topic state remain equal. Clock/actor simulation continued normally; this
is not an immediate pre-frame actor snapshot comparison. The captured entrance shows the original
four portraits, minimap and Chinese label. This checks the completed directory after its rename,
not only the temporary path validated by the converter.

Mac native builds and generic unsigned iOS Release pass (`build-003.log`, `ios-release-001.log`).
No game logic or assets were changed in this checkpoint. Keep using 063 as the primary runtime.
`app-support-001` is the retained complete app-import fixture for future cancellation/history/storage
work; reuse it rather than reimporting the same source into another copy.

## Evidence and remaining scope

Evidence root: `../local-data/validation/import-recovery-069/`. See `result.json`, `tests-003.log`,
`actual-interruption.json`, `actual-retry-comparison.json`, `copied-source-hashes.json`,
`published-startup-comparison.json` and `published-startup-001/scene-first.png`.

The first test run had 18 passes and one failure exposing the missing error detail; it is not the
final pass. The first real-source audit script stopped before conversion because the system Python
lacked `hashlib.file_digest`; streaming SHA-256 replaced that diagnostic call.

Recovery discards unfinished work and allows a retry. A cancel button, byte/stage resume, import
history/storage UI, power-loss durability and iPad file-provider/low-space/background behavior remain
open. The supplied directory still lacks `Data/bitmaps.lod`, `sprites.lod` and `icons.lod`; MM8 base
artwork, MM8/rift scene integration, other gameplay/media gaps and long-session/device acceptance remain
unfinished. Overall port progress stays at an estimated 40–45%, with low confidence.
