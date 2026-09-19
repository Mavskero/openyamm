# Resource gap reporting across conversion and import — checkpoint 073

2026-09-11. The launcher previously displayed missing MM8 base archives only while the active
selection was an original source directory. It suppressed the warning after native conversion;
reimporting a portable runtime also discarded the source dependency information. Checkpoint 072's
black chest area made the resulting misleading startup state concrete.

The conversion manifest now preserves a bounded resource audit, and the Foundation importer carries
it into its selection record. The launcher displays its status for original and converted imports.
Known gaps leave a newly completed conversion on the launcher, with a manual continue-testing
button. The original directory's action is labelled Convert Resources when those gaps are already
known. No existing runtime, save or source files are deleted to enforce this status.

## Exact assessment scope

`conversion.json.resource_audit` schema 1 records:

- Presence of three regular source files: `Data/bitmaps.lod`, `Data/sprites.lod`, `Data/icons.lod`.
- Presence of the eight shared `icons/chest01.bmp`–`chest08.bmp` backgrounds in the assembled engine
  immediately before its work directory is removed. These are the original MM8 presentation assets,
  not prefixed MM6/MM7 substitutions.
- Separate checked flags and missing-path arrays for each category.

This is file-presence assessment and retained conversion evidence, not a full inventory of missing
assets, a fresh integrity scan of an imported ZIP, or validation of all artwork/audio/gameplay.
An empty list does not set runtime_ready/runtime_validated/full_gameplay_validated to true. The
launcher explicitly describes it as what the conversion record reports and retains the development
scope. Complete MM8 scene and MMExtension gameplay integration is still unfinished.

Original imports assess the base files directly; chest checking remains unperformed until conversion.
Directories named like base archives do not count as files. Portable reports are limited to 1 MiB,
validated against the known schema/path lists, and stripped of arbitrary display text before storage.
Damaged, absent, oversized or future reports produce unknown status, not a claim of completeness.
Old native records retain their previously recorded nonempty missing-base list. Unknown status does
not prevent manually testing an otherwise loadable older runtime.

`apple_import --status SUPPORT_DIRECTORY` reads the same Foundation status used by the launcher,
without changing the active selection. Launcher wording and post-conversion behavior compile for
iOS, but UIKit presentation and real device input have not been exercised in this checkpoint.

## Verification

Evidence: `../local-data/validation/import-resource-status-073/`.

- Final native regression: 40 cases pass (`tests-003.log`, `tests-003.xml`), including the earlier
  copy/recovery/cancellation cases plus status preservation, legacy/unknown records, malformed data,
  duplicate/unrecognized paths, directory-vs-file detection, readonly status and updated reimports.
- Updated-profile tests use clearly labelled metadata/ZIP fixtures. They do not demonstrate a
  complete real MM8 installation. An initial malformed-report test parameter generated an oversized
  pytest environment value; named parameter IDs fixed that test harness failure. It was not an
  importer failure; the initial log is retained.
- The actual retained 069 source was converted through the native API: all 140 currently supported
  map loads passed. Its report identifies the three missing base archives and eight missing chest
  backgrounds. The report describes the same source gaps established by the previous resource audit.
- Native copying of that complete generated directory into a separate app-support fixture preserved
  the audit and status message without a source_id/source directory. All four ZIP hashes match the
  retained 069/063 packages; the portable copy also retains the exact conversion manifest bytes.
- All 377 retained source files still hash-match the user's original files. The prior 069 active
  selection is restored byte-for-byte; only the newly created duplicate runtime and portable copy
  were pruned after comparison. Reports, records and hash receipts are retained in `actual-result.json`.
- Final native build and generic unsigned iOS Release pass (`build-002.log`, `ios-release-002.log`).
  The actual incomplete-source conversion used the preceding binary; the only later native change
  clarifies the no-known-gap message. Final tests exercise the updated message as well.

No gameplay code, game data, ZIP payload or save-format change is introduced. The primary runtime
remains checkpoint 063. This round did not repeat game rendering tests for byte-identical ZIPs.
All operations and generated files stay on NVMeSSD; no physical iPad or simulator UI operations.

Overall estimate remains **40–45%, low confidence**. This closes a startup reporting gap; it does
not complete the missing chest interface or the remaining campaigns, device acceptance and stability work.
