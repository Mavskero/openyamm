# MMMerge NPC rules import profile

`npc_import SOURCE BASE_ASSETS NEW_DESTINATION` exports six shared tables:
professions, random names, personality rules, and area/continent/profession news.
It reads the original loose tables and Global.txt directly from the source LODs;
NPC dialogue/news and map registries must already exist in BASE_ASSETS. Output
is UTF-8, mode 0644, published to a fresh directory outside both inputs. Original
source files are unchanged and no scripts or Windows code execute.

## Identities and localization

All six original tables retain every source cell and order. Profession IDs must
match their zero-based row position; profession news IDs must match their
one-based row position, reflecting the source Lua loaders. Zero is the original
empty profession. The supplied source has 78 professions, 13 personalities,
540 male names and 310 female names. Name order and repeated names are retained.
The supplied loose random-name table is English and has no matching localized
archive table; this phase does not claim to translate those names.

The profession export appends a named Display Name column. Seventy-seven labels
come from the matching IDs in the original Chinese Global.txt. Base Global.txt
must first agree with the English profession label; numeric identity, English
mechanical keys, personality keys, prices, rarity and flags remain unchanged.
The decoder restores 3,738 glyphs in the complete Global.txt input; that is not
the number of glyphs exported in profession names.

The shared loader recognizes only the named extra column. Existing tables and
programmatically constructed entries retain their English fallback. Follower
views, profession text substitution, fallback action labels and generated actor
titles now use the display name; scenario semantic keys retain the base name.
Every nonzero profession passes the production follower-view check for its
display name, ID and fee. This does not accept hiring interaction or fee timing.

## News references

The output retains 263 area entries, 20 continent entries and 539 weekday entries.
Unprefixed IDs select NPCNews/NPCNewsTopics. Four area entries instead explicitly
select NPCTopic/NPCText. The shared area/continent loader now retains these source
flags. Generic actor resolution, indoor/outdoor dialog opening and rebuilding
keep the explicit body source. A focused collision case puts different text at
the same ID in both registries and checks that the Chinese NPCText body survives
dialog reconstruction. Explicit NPCText references never fall back to NPCNews.
The prior unprefixed legacy fallback remains available for existing engine data.

All referenced maps, continents, titles, bodies, profession actions and the
personality table's 24 text fields resolve against their appropriate registries.
Personality acceptance flags, fame and signed reputation requirements round-trip.
The current news selection still uses existing engine behavior, including the
first area/continent entry fallback. This phase does not reproduce the original
per-NPC random news assignment or prove every entry is reachable interactively.

Three profession-news notes differ from current profession labels: 64 says
Stonecutter instead of Stone Cutter; 74 says Gambler instead of Follower of Baa;
76 has the inverse pair. Source code indexes these rows by position. Their
labels and numeric weekday associations remain untouched and the differences
are reported; no speculative content swap is performed.

## Verification

`npc-tables-002` independently matches all original cells and Chinese display
labels. Sixteen focused tests and the complete 342-test resource suite pass.
Mac game/converter and iPadOS arm64 library/app builds pass. Combined checkpoint
`gameplay-base-018` passes these six tables during formal startup, which next
requires `monster_portraits.txt`. Optional house animations remain missing.

No iPad was connected or updated. Actual follower hiring, begging/bribing/threats,
all news selection rules, rendering, random-name translation and full gameplay
remain unvalidated. This import adds no images and does not change icon ownership.
