# The level manifest's base class: its words, Finished and seven kinds of item (subsystem `gm/dbmanifest`), 2026-11-18

For the integrator: this holds what belongs in docs/formats.md, docs/verification.md, docs/STATE.md and docs/startup.md. Reference build knd-xbox-usa-5454008a-v1. State of this work: Semantic-tested. A host MATCH from `tools/verify.py` says nothing about the original compiler.

**What a green result does not show, first.** The oracle runs the original's code for everything recovered here, and both sides parse every real `.MNG` file to the end and run every item. But what the items load into is, on both sides, either a double or this file's fakes: the loads of scene, cinema and event databases (0x0002F7F0, 0x000C5B30, 0x000C9A00) are doubles that fill in fake arrays; the registry names' Add and Release (0x00071F20, 0x00071DC0), the sound manager's FreeDatabase (0x00139F90) and the space graph's setter (0x00140E40) are doubles; the makers of sound databases and of graphics database loaders are the oracle's classes, and what they make are the oracle's subclasses of the recovered `AtAudioDatabase` and `AtDatabaseLoader`, whose slots only log. The level's own items and what they load into are doubles as in `oracle_manifest.cpp`. What runs for real on both sides: the parser, the queue, the items, `AtAudioDatabase::Load`, `AtDatabaseLoader::Load` and `AtBoundingGeometryDatabase::Load` on empty written files, and `AtDataRegistry::Load` on a written registry. So a green result says the base's words, items, their teardown and Finished agree with the original's; it says nothing about what a real `.AOD`, `.AGD`, `.ASD`, `.AEE` or `.ADR` holds.

## What was recovered

60 functions and 10 vftables, in `src/gm/GmDatabaseManifest.cpp` and `include/gm/GmDatabaseManifestItems.h`; records in `config/functions.d/dbmanifest.toml`.

* `GmDatabaseManifest::OnToken` 0x0015D000 (the seven words, jump table 0x0015D900) and `Finished` 0x0015D920.
* Seven item classes, each with constructor, destructor, deleting destructor (compiler-made, for arrays too), slot 0, `Run` and `Configure` (42):

| Word | Item | Ext | Vftable | Size | Holds; Run loads through |
| --- | --- | --- | --- | --- | --- |
| SoundDatabaseLoaders | `GmSoundDatabaseItem` | AOD | 0x002950E0 | 0x9C | a database from the maker at 0x003B3550 (slot 5), `AtAudioDatabase::Load`, then `GmAudioData::RegisterDatabase`. Runs during the parse. |
| GraphicsDatabasesLoaders | `GmGraphicsDatabaseItem` | AGD | 0x002950EC | 0x98 | a loader from App+0x5F90 (slot 29), `AtDatabaseLoader::Load(owner, path, mode, 0, 1)` |
| SceneDatabasesLoaders | `GmSceneDatabaseItem` | ASD | 0x002950F8 | 0x10C | an `AtSceneDatabaseLoader` in place (at/asd's) |
| CinemaDatabaseLoaders | `GmCinemaDatabaseItem` | ACD | 0x00295104 | 0xB8 | an `AtCinemaDatabase` in place |
| BoundingGeometryDatabase | `GmBoundingGeometryItem` | ASB | 0x00295110 | 0xBC | an `AtBoundingGeometryDatabase` in place (at/asb) |
| EventDatabases | `GmEventDatabaseItem` | AEE | 0x0029511C | 0xB8 | an `AtEventDatabase` in place |
| InstanceRegistries | `GmRegistryItem` | ADR | 0x00295128 | 0x288 | an `AtDataRegistry` in place (at/adr), then the registry names at 0x002CD138 |

* `AtCinemaDatabase` (vftable 0x00286474): constructor 0x000C5780, `Clear` 0x000C57C0, destructor 0x000C5A00, deleting 0x000C5AD0. `AtEventDatabase` (0x00286688, a `GmNameMap`): constructor 0x000C9660, `Clear` 0x000C96D0, destructor 0x000C9960, deleting 0x000C5720. `AtEventList` (0x00286674, the array an event database keeps): constructor 0x000C94B0, `Free` 0x000C9470, destructor 0x000C94D0, scalar deleting 0x000C9600; its `Reset` is 0x000E7880, folded, in `[symbols]`.
* `AtSceneDatabaseLoader::InsertInto` 0x0002ED70 and `RemoveFrom` 0x0002EDD0 (at/asd did not recover these two); `GmLevelManager::FindSpaceGraph` 0x0013D9D0 (0x003B3570 is `GmLevelManager::s_instance`); `GmAudioData::RegisterDatabase` 0x00137AD0.

`GmDatabaseItemsDelete` (declared by gm/manifest) is now real: the original's inline `delete[]` of each base array, by type.

## Seams, and what each item's Run stops at

In `src/gm/GmDatabaseManifestHost.cpp`, each stopping with a diagnostic: `AtSceneDatabaseLoader::Load` 0x0002F7F0, `AtCinemaDatabase::Load` 0x000C5B30, `AtEventDatabase::Load` 0x000C9A00, `GmRegistryNames::Add` 0x00071F20, `GmSpaceGraphOwner::SetGraph` 0x00140E40. Still in gm/manifest's host file: `GmAudioFreeDatabase` 0x00139F90 (the sound manager stops voices on every emitter for each bank, a device call) and `GmRegistryRelease` 0x00071DC0.

Classes of other branches, declared here so that this branch builds; each is marked STANDS FOR in the header, and folding means deleting the declaration here:

* `AtSceneDatabaseLoader` (at/asd, `include/at/AtSceneDatabaseLoader.h` on integrate-batch5), with that branch's member names. Its constructor, destructor, `Clear`, `SpaceGraphAt` and `LoaderAt` are that branch's: here they are stand-ins in `src/gm/GmDatabaseManifestAsd.cpp`, written to the original's code so that scene items can be made and freed, compared against the original's in the oracle, and recorded nowhere. When at/asd is merged: delete that file and the declaration, and move `InsertInto` and `RemoveFrom` into its class.
* `App` (start-up's, `include/gm/GmApp.h` on integrate-batch5): only `m_graphics` (+0x5F90), `m_unknown5F94`, `m_materialOwner` (+0x5F98), the scene owner at +0x60E8 (inside its `m_unknown5F9C`) and the cinema owner at +0x635C (at +4 of its `m_userInterface`). Reached through `GmHistoryLogs::m_app` (+0x0C).
* `AppGraphics` (start-up's, slots 0 to 17 there): slot 29 `NewLoader` and slot 30 `DeleteLoader` are what this adds. The concrete loader it makes (vftable 0x0028A7D0) is the graphics branch's.
* `GmRegistryNames` (start-up's, 0x002CD138), with `Add`.
* `GmAudioDatabaseMaker` (0x003B3550; slot 5 makes, slot 6 frees; the Xbox database it makes, through 0x000E9E90, is another branch's). New name; no branch had one.

## What a real build does on real files

Driven as the host test drives it (the objects start-up would make supplied), all 51 real `.MNG` files now parse to the end and queue every item: 285 sound, 1158 graphics, 1332 scene, 59 event and 233 registry items, and the 887 level items. Sound databases load during the parse in the 46 files that have the word; each goes to the maker, which is another branch's (with none, an item counts as run and loads nothing, as the original does when the maker or the owner is null). Where the queue then first stops, by file (queue order: events, registries, sound, graphics, bounding geometry, scenes, cinema, then the level's):

* 31 level files: the first event item, at `AtEventDatabase::Load` 0x000C9A00.
* 18 character files: the first graphics item, at the graphics branch's `AppGraphics::NewLoader` (slot 29); with it, the recovered `AtDatabaseLoader::Load` reads the `.AGD`.
* `PERSIST.MNG` (start-up waits on it): its only word is GraphicsDatabasesLoaders, so the same as the character files. Start-up's wait now gets past the parse.
* `UI.MNG`: no base word after the parse (its sound databases load during it); its one GOM item then runs into the level's music seam.

A real build without start-up's objects crashes at the first GraphicsDatabasesLoaders or SceneDatabasesLoaders word, reading `GmHistoryLogs::s_instance->m_app`: start-up (0x0008E660) is not on master.

## Oracle

`tests/oracle/oracle_dbmanifest.cpp`, suite `dbmanifest`, built from `oracle_manifest.cpp`'s doubles of the level's items. Cases: every real `.MNG` through each side's own `Update` (open, parse with the sound databases loaded on the way, every item run, Finished), then again, then destroyed; each base word with none, one and three paths; all words together with two level words; the sound maker answering null from the first and from the third; no sound owner; no sound maker; the language `%s` (once, twice, none, and a nine-character language cut to eight); platform binaries off (graphics mode 0); the scene paths' flags in each combination, a missing flag, no current graph; a word twice; the words the base lets by, a loose brace; paths of 127 and 128 characters (see below); seven syntax errors; parse and queue only; no heap and no memory for each word; the sound registry full; Finished alone with graphs put by hand in a database's own loader, in a nested loader's nested loader (depth first), nowhere, with and without a current graph, then the search on each database and the two walks by hand; the base's Free alone with three of each kind, with and without a graph; each item class alone (made, configured from per-side requests, run, slot 0, run again, deleted with flags 1 or destroyed with flags 0; the sound item with the maker answering null); the cinema and event databases alone (made, filled, cleared, filled, deleted or destroyed), the event database's Clear with a block and no list and with a list and a name map of three, and the event list alone. Each graphics item's loader lists one mesh database of the oracle's, which reports whether it was asked for a binary form (mode 2) and the depth of the file system's directory stack when it is loaded (the directory flag); scenes and graphs are logged by what they are, so the order of the two walks is seen.

Result: `NO DIFFERENCE OBSERVED: 51 files; 318516 comparisons, 0 differences`; real files run to the end on both sides: 51 / 51; entries reached 60 / 60; harness mutation exits 1; mutants detected 130 / 130. The first mutant run left 20 alive and the second 2; each was a gap and is closed, except three dropped as equivalent with the reason in `tests/oracle/suites/dbmanifest.py` (the language copied by `AtCopyName9` straight from the source; `DeleteObjects`' own test of a count of 0, which the host compiler's `delete[]` makes itself; and one rewritten that could not be told apart, `|| m_ran` inside a branch where `m_ran` is 0). The gaps: Finished's scenes were given as one word twice, so only the last array existed (and the hand-made graphs were written past it); scenes were logged by a place that was the same for every node; a 128-character sound path was never parsed; nothing showed the graphics loader's mode or directory flag; destructors ran on items already cleared by slot 0; the event list's count and the event database's Clear with no list were not compared. Host tests: 33 / 33 pass on x64, test_dbmanifest among them. `tools/verify.py --only` on the 60: 4 MISMATCH, 56 SIZE, none MISSING or UNRESOLVED; 10 / 10 vftables MATCH.

Not compared, or not run:

* Sound database paths of 127 and 128 characters: the item opens a file named by the path at once, during the parse, and the name and its extension overrun the open's 0x80-byte buffer on both sides (as gm/manifest found for the notes). Graphics, bounding geometry and registry items with such paths are parsed and queued, not run, for the same reason.
* With platform binaries off, the graphics case reads an empty `.AGD`: with mode 0 the mesh would be read as text, which is the mesh database's work (at/agd) and differed between the sides there in a first try; not investigated here.
* A 128-character path's terminator clears the low byte of the request's first context. The oracle's makers, owners and App are aligned to 256 bytes so that both sides keep the same pointer.
* A registry item is loaded once per case: loaded twice, a registry is attached under its parent twice, and the original's code then walks for ever. A graphics or bounding geometry item is not run after its slot 0, which forgets the maker or owner that Run then uses unchecked (a crash on both sides).
* The event and registry items' constructors leave the owner or manager unset; both sides see the heap's fill byte.

## Quirks kept

* SoundDatabaseLoaders runs each item as soon as it is configured, inside the parse; its paths are limited to 128 characters, the other words' to 127.
* A sound database path is a printf format: `%s` becomes the language's directory (0x003B3BE4), copied as an eight-byte name. A path with any other `%` reads garbage arguments.
* After a scene path, the next token is read whatever it is (InSpaceGraph sets the flag, anything else clears it); then InsertInSnoopie is read only if it is there. A path with neither flag swallows the next path, which then meets a keyword where a string should be.
* Finished makes the first space graph found the current one (each database's own first, else its nested loaders' depth first), and then inserts every InSpaceGraph scene into the current graph, whichever it is, found or not.
* The graphics item does not test the loader it is given; with platform binaries on (0x002B9348), mode 2.
* The sound item gives its database back only if the database has an owner; with no maker or owner it loads nothing and still counts as run.
* The registry item builds its unused "%3s][%s" after loading, the others before.
* `AtEventDatabase::Clear` does nothing at all, the name map's own Clear included, when its list holds no array. `AtCinemaDatabase`'s destructor clears always; the scene loader's only when it has an owner.
* The event item's slot 0 clears the path's first byte and keeps the owner; the registry item's keeps the manager.

## Shared edits

* `include/gm/GmLevelManifest.h`: the header comment and the base's declarations now point here; `GmSpaceGraphOwner::SetGraph` 0x00140E40 declared; `GmDynamicNodeState` gains `m_dataOwners` (+0x04) and `m_registry` (+0x2C, an `AtDataRegistry`), its `m_unknown00[0x24]` becoming `m_unknown00` and `m_unknown08[0x1C]` (nothing used it). Start-up's branch calls this object `GmDataRoot`, with the same two members: one class, to fold. Includes `at/AtDataRegistry.h` now.
* `src/gm/GmLevelManifest.cpp` (gm/manifest's): the base's AfterParse and Free use the item classes instead of x86 strides and offsets (`ItemAt(m_events, i, kEventItemSize)`, `item + 0x108`, `+0x8C`, `+0x90`), so that they are right on x64; same addresses on x86. The manifest suite's ten mutants of those lines are rewritten to the new text with the same mutations (`tests/oracle/suites/manifest.py`; "base queue: stride" is now "base queue: index").
* `src/gm/GmLevelManifestHost.cpp`: its seams for `OnToken`, `Finished`, `GmDatabaseItemsDelete` and `GmSceneItemLeave` are deleted, being recovered here.
* `config/functions.d/manifest.toml`: the two `[symbols]` lines saying `OnToken` and `Finished` are not recovered are removed; `GmSceneItemLeave`'s comment says it forwards to `RemoveFrom`.
* `include/gm/GmAppSettings.h`: `GmHistoryLogs::m_unknown[0x18]` is now `m_unknown00[0xC]`, `App* m_app` (+0x0C), `m_unknown10[8]` (nothing used the old name).
* `include/gm/GmAudioData.h`: `RegisterDatabase` 0x00137AD0 declared.
* `include/gm/GmLevelManager.h`: `FindSpaceGraph` 0x0013D9D0 declared.

## What to recover next

1. Merge at/asd and start-up's branch, then fold the STANDS FOR declarations above (deletions) and `GmDataRoot`/`GmDynamicNodeState`.
2. The event database's Load 0x000C9A00 (`.AEE`): every level file stops there first.
3. The sound manager's FreeDatabase 0x00139F90 and the registry names (0x00071DC0, 0x00071F20, 0x00071E50).

## My mistakes

* I first declared the scene database as a class of my own, with its constructor, Clear and destructor as recovered work; at/asd had recovered the class on integrate-batch5. Renamed to theirs, with their member names, and the duplicated functions made unrecorded stand-ins.
* My first oracle mapped a pointer to the first registered block holding it; freed blocks are handed out again, also to the other side, so it matched stale ones. Now the newest block on either side wins.
* The base's AfterParse and Free stepped through arrays by x86 sizes: the x64 host test crashed on the second sound item. Fixed by typing them (shared edit above).
