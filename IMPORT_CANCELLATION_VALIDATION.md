# Import cancellation — checkpoint 070

2026-09-11. Evidence: `../local-data/validation/import-cancel-070/result.json`.
All work stays on NVMeSSD. No physical iPad operations or simulator UI interaction.

## User-visible behavior

The launcher shows a Cancel button while copying or converting resources. A copy reports completed
file count and bytes copied, updating at most ten times per second. Cancel disables that button and
keeps the spinner until the worker finishes cleanup; queued progress cannot replace cancellation text.
The existing selected resources remain available after an accepted cancellation. A subsequent attempt
starts from the beginning; this is not byte or stage resume.

Cancellation is sealed before the final directory rename and atomic selection write. If publication
has already started, Cancel returns false and the UI waits for publication to finish. A late user
request suppresses automatic game launch even if conversion completed successfully. It does not
revert a completed import. The existing transaction rollback still handles publication failures.

## Implementation

- Each operation owns a thread-safe `OYImportCancellation` token, with an atomic transition between
  running, cancelled and publishing. Tokens are not reused between operations.
- Source files are copied through a 64 KiB buffer with cancellation checks between reads and writes.
  Short writes/EINTR are handled, source file type/size is checked, and the existing byte/file limits
  remain. Copies are ordinary resource data; the root mm8.exe remains nonexecutable and is never run.
- Native runtime conversion checks cancellation between stages, while merging files, in event/map
  loops, while exporting scene images, in ZIP entry/chunk loops, and between map validation loads.
  A typed cancellation unwinds cleanup and reaches Foundation as `NSUserCancelledError`.
- The launcher retains its `NSFileCoordinator` and cancels a waiting coordination request, while the
  importer stops an accessor that is already copying. This integration follows the local SDK header;
  actual external/cloud file-provider waiting behavior has not been exercised here.
- Some converters still finish their current stage, and an individual filesystem/decoder/map-load
  call must return before cancellation is observed. No fixed maximum cancellation latency is claimed.
  Crash journals, flock and startup recovery from 069 continue to protect interrupted work.

## Verified

`tests-001.log` / `tests-001.xml`: 26 passing native tests. These include all 20 previous recovery
cases and six new cases: pre-cancel with no readable source, no cleanup when pre-cancelled, mid-file
cancellation/retry for both source and runtime directories, cancellation before the first conversion
stage, and rejection of cancellation after successful publication. The two large-file cases verify
bounded progress, unchanged active bytes, cleanup, retry and byte-identical copied files.

`validate_actual.py` / `actual-result.json` use the existing complete 069 app-support fixture:

| Operation | Result |
| --- | --- |
| Actual source copying cancelled after at least 1 MiB | Cancel accepted; prior runtime selection byte-identical; no new output left |
| Actual scene conversion cancelled after processing starts | Live scene output observed; cancellation and cleanup pass |
| Actual ZIP packing cancelled after processing starts | Nonempty engine.zip observed while live; cancellation and cleanup pass |
| Actual map validation cancelled after processing starts | Complete engine.zip present and map loading logged; cancellation and cleanup pass |
| Complete retry without cancellation | 140 maps load; four ZIPs hash-identical to the retained 069/063 runtime |

The test temporarily selected the retained source for conversion, then restored the original runtime
selection byte-for-byte. All 377 retained source files still hash-match the user's original files;
the original four runtime ZIP hashes also remain unchanged. This is not a second complete copy of
the real source through the new copier: that copier's successful full-file checks use test fixtures.

The new duplicate runtime was removed only after its successful hash comparison and restoration of
the previous selection. Retained source/runtime IDs remain 31DF6924-1859-4029-B133-6205E7E5823F and
0336B2DA-2461-4468-8935-8439BA9CC615. No staging or journal output remains from these cancellations.

Generic unsigned iOS Release builds successfully (`ios-release-001.log`), including ResourceImport,
Launcher, IOSMain and the changed converters. The only later production edit wraps one declaration
without changing non-whitespace text. Early build failures (missing venv PATH and Objective-C ivar
initializer syntax) are retained in build-001/build-002 logs; build-003 is the successful Mac build.

No resource format, gameplay rule or save schema changed; the existing 063 package remains usable.
UIKit button interaction, actual file providers, low space, background suspension and power loss
still need verification. This checkpoint does not establish additional gameplay/campaign coverage.
Overall estimate remains 40–45%, low confidence.
