# GOG MM8 source extraction — checkpoint 075

The user supplied the English Windows offline installer
`setup_might_and_magic_8_day_of_the_destroyer_1.0_(22833).exe`. It was successfully extracted on
macOS without installing Windows or Wine, or executing the Windows installer/game.

- Installer: 641,778,744 bytes; SHA-256
  `1d5aa5d1aca67504c6ffd719a47bab40e1990b4fe818e106c086efb77fc9ddc5`.
- Stored in `../external-storage/original-game/MM8-GOG-downloads/`; the Downloads entry is now a
  compatibility symlink after copying and comparing SHA-256, so it does not retain a second internal-disk copy.
- Extracted source: `../external-storage/original-game/MM8_GOG_1.0_22833/`.
- All 175 extracted files, totaling 665,003,314 bytes, match their installer MD5/SHA-1 checksums.
  Separate SHA-256 receipts are retained for every file. Extraction completed with exit 0.

The extractor is innoextract 1.9 from the official Homebrew arm64 Tahoe bottle. Its verified bottle
and seven required Boost libraries were unpacked and relocated under `../external-storage/tools/innoextract/`.
Existing macOS/xz/zstd libraries are reused. Relocated Mach-O files were signed after changing their
load paths. Both original bottle hashes, formula metadata, licenses and relocation receipts are retained;
downloaded bottle caches were removed. No global Homebrew package or Windows environment was installed.
See the [innoextract project](https://constexpr.org/innoextract/) for supported installer extraction.

## Source contents verified

| Archive | Entries | Result |
| --- | ---: | --- |
| Data/bitmaps.lod | 1,818 | Original base textures present |
| Data/sprites.lod | 10,688 | Original base sprites present |
| Data/icons.lod | 3,943 | Includes all eight chest01–chest08 backgrounds |
| Data/games.lod | 122 | Original map archive; entry count is not map count |
| Data/EnglishD.lod | 3,450 | Includes 3,250 WAV entries |
| Data/EnglishT.lod | 194 | Original English table archive present |

Both `Anims/mightdod.vid` and `Anims/Magicdod.vid` match installer checksums, as do fourteen original
MP3 music files. LOD signatures, index bounds and unique entry names were inspected. This is source
extraction/integrity evidence, not a claim that every archive entry has been decoded or played.

Correction to the earlier source assessment: this GOG edition stores audio in `Data/EnglishD.lod`.
Its lack of a separate `Sounds/` folder is not a missing-file error. The current sound importer reads
only prefixed Merge/Chinese archives and must explicitly add this original base layer with appropriate
override ordering. Preserve the source's `Magicdod.vid` spelling; ensure import works on case-sensitive volumes.

## Integration still pending

Compose the MM8 base, the existing Merge version and the existing Chinese overlay in a separate
source directory, preserving both originals. Regenerate and validate the chest interface, MM8 scene
resources, original audio/video and Chinese precedence. The retained checkpoint 063 runtime has not
been rebuilt; its known-gap report is still accurate for that old package. This extraction does not add
an EXE importer to the iPad app or complete MM8 gameplay.

Checkpoint 074's candlestick diagnostic is unfinished: realtime retreat failed, and the revised
turn-mode retreat compiles in `build-003.log` but has not run. No physical iPad/simulator UI operations
occurred. Overall gameplay estimate remains 40–45%, low confidence.

Evidence: `../local-data/validation/mm8-base-075/{result.json,installer-list.txt,extraction.log,
extracted-file-checksums.json,lod-audit.json,tool-relocation.json}`.
