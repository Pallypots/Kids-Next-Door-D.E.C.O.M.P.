# STATE — read this first (with CLAUDE.md), then `git status`, build/verify-report.json, build/oracle-report.json

Last updated: 2026-11-07, dev cycle 22 (second parallel batch: AGN family, AKC, sound databases, input mappers, Flash text, actors). Per-function results live in docs/PROGRESS.md (generated).

## Milestones

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 Identity | **passed** | docs/provenance.md: input hashes, both containers parsed, identical XBE `f5f48a6f…`, retail key, 17/17 section digests, XDK 5849 |
| M1 Trustworthy tools | **partial** | inventory (tools/funcs.py), relocation-aware and vftable checker with negative controls, differential oracle qualified with source mutants, Ghidra cross-check done (docs/verification.md); **original toolchain blocked** |
| M2 Reference reconstruction | blocked | needs an original-era MSVC (docs/toolchain.md) |
| M3 Verified subsystem | **partial** | `gm/containers` and the archive layer recovered with type documentation, host tests and behaviour evidence against the original code; no original-compiler match, so "matching evidence" stays open |
| M4–M7 | untested | — |

## What is recovered

All source is in `src/`, types in `include/`. 978 functions and 78 vftables are recorded in `config/functions.toml` and `config/functions.d/`.

* `gm/containers` (47 functions): socket/plug connection model, connection base, pooled linked-list node, linked list,
  iterators, one template instantiation. Complete for these classes apart from the freeing path noted below.
* `at/jam` (101 functions), **partial**: info header, name table, directory lookups, header and file classes, `AtIoPath`, the base
  `AtIoFileSystem`, the file system stack of `AtIoContext`, and `AtIoJamFileSystem`. **Recovered source reads every file of every archive on the
  disc** on a 64-bit host, over a host file layer in `platform/`, which is port code. **Not recovered:** directory listing (`AtIoDirectoryListing`,
  the `List` and `FreeList` slots), the JAM1 class (no archive on this disc uses it), the rest of `AtIoContext`, the Xbox device file system, and
  the heap and error helpers. Unrecovered slots stop with a diagnostic in `src/at/AtIoJamPending.cpp`, which is never recorded and never counts;
  their vftable slots are checked by position only. `platform/fatal.cpp` stands in for the helpers.

* `at/text` (99 functions): the stream and file wrapper, and the tokenizer most text formats go through, both reading and writing. It works on 64-bit, with the same results as the
  original on every text file on the disc.
* `at/mount` (23 functions): what mounts archives, and the small manager it creates. A port supplies the factory of file systems. Start-up order: docs/startup.md.
* `gm/audiodata` (3 functions), **partial**: the volume preset (`.GOV`) and memory block (`.GOB`) readers of the audio manager, which is not itself recovered. `MEMBLOCK.GOB` on this disc has only `PSX2` and `GCN` sections, so the Xbox game takes no blocks from it.
* `at/heap` (22 functions): the allocator, a manager carving up to 16 heaps out of one region, first-fit heaps with coalescing free lists, wider-aligned allocations, groups of heaps standing in for each other, and the report. The arena of the original and the recovered source stay identical byte by byte through long random runs.
* `at/memory` (19 functions, 2 vftables): the stack of heaps, the original's global new and delete, the error routine, and the object that takes a region from the platform, starts the manager and makes the default heap. A port can now start the allocator, as the host test does. **Recovered code does not use it yet**: `new` still reaches the host's allocator, by decision, since the host's library shares the process (`include/at/AtMemory.h`). Which concrete object the game starts, and with what sizes, is not recovered: its class at 0x00141840 holds this as a member. `AtFatal` stays a stand-in, since the original reaches into graphics and sound.
* `at/material` (14 functions, 1 vftable): the material as read from `.AGM` files, and its keyword table. Reads every material on the disc the same as the original. Not recovered: the concrete material at 0x0028A204 that passes this to the graphics device.
* `at/matdb` (12 functions, 1 vftable): the database that loads an `.AGM` file, its texture lists, and the owner's lookups for shaders and materials. Loads every `.AGM` on the disc the same as the original. Not recovered: the concrete database at vftable 0x00289300 in the graphics layer, and the rest of the owner.
* `at/agt` (13 functions, 1 vftable): the texture database, the loader of `.AGT`. Not recovered: the concrete database in the device layer and the items' own reader, so what a texture holds is not read yet.
* `at/aob` (40 functions, 4 vftables): the sound bank and segment lists, `.AOB` and `.AOS`. Not recovered: the concrete banks, so nothing here plays a sound.
* `at/ags` (33 functions, 2 vftables): shaders, `.AGS`, with staged and programmable components and texture stages. Not recovered: the concrete shader and the AGV and AGP loaders that fill the process groups. The disc's text cases hold only staged components, so the programmable path rests on written text alone.
* `at/asn` (85 functions, 7 vftables), **partial**: scenes, `.ASN`: the scene, the base and modifier nodes, the coordinate systems and their database. **A real build stops with a diagnostic on nearly every real `.ASN` file**, since nine classes of node are not recovered (`src/at/AtSceneActorsHost.cpp`). Also not recovered: node slots 12 to 31, 0x0003CE10, the modifier set beyond its removal functions, and the matrix table at 0x002C60C8, which the graphics layer fills.
* `AtNameTableOwner` (`include/at/AtNameTableOwner.h`, vftable 0x00274A78 not recovered): what sound lists and scenes load for. Two branches named it separately; folded at integration on the evidence given in that header.
* `at/agn` (85 functions, 8 vftables): `.AGN`, `.AGA`, `.AGI`, `.AGF`, `.AGL`, `.ASL`. Images and fonts are compared at database level only: harness items stand in for their bodies. The three real `.ASL` files on the disc are not this format, so the lens flare rests on written text.
* `at/akc` (20 functions, 1 vftable): the `.AKC` reader, one `AtCoordSysDatabase` a file, and the base class of textures. A real build still stops at the matrix global and the concrete textures.
* `at/aod` (66 functions, 7 vftables): sound databases `.AOD` and `.AOE`, with the Xbox lists. A real build stops at the first bank that needs `DelayLoad` (the XACT seam in `src/at/AtSoXboxHost.cpp`); the factory 0x000E9E90 is not recovered.
* `at/ana` (40 functions, 4 vftables): input mappers `.ANM` and the input router. **`.ANA` is not recovered.** Binding is not recovered, so no real file reaches the unbinding code.
* `at/afx` (21 functions, 4 vftables): Flash text formats `.AFX` and level files `.AFL`. The AFD reader 0x00011770 and most of `AtFlashLevel` are not recovered.
* `at/actor` (60 functions, 3 vftables): actor base, ModelActor, JointedModelActor, animation set, blend, channels, database copy. Replaces seams that stood for them. Running-game slots stop with a diagnostic. **A real x64 build, by the agent's scan, loaded 430 of 652 distinct `.ASN` texts and cleared them with nothing reported; the other 222 stopped at a node class not recovered. That is not a level loading in a game: the device and its tables are stand-ins, and nothing is updated or drawn.**
* `at/pixelformat` (27 functions): the texture format descriptor, whose flag names are in the binary: `RGBA`, `LUM`, `INDEX`, `BUMP`, `DEPTH`, `FLOAT`, `DXT1` to `DXT5`, `SWIZZ`, `TILED`, `LUMD`. x64 gives the original's results.
* `at/quaternion` (11 functions, 2 of them on `AtVector3`): product, lerp, slerp, from rows of a matrix, axis and angle, rotating a vector. The enclosing kinematics class from 0x00033030 is not recovered.
* `at/vector` (14 functions): the 3D vector, called from all over the engine. Gives the original's bits on x86 and, for the functions without a sine or cosine, on x64 too. **The game's main thread runs at x87 precision 53**, read from the binary: docs/portability.md.
* `gm/loader` (10 functions), **partial**: the base of the time-sliced `.MNG` loader start-up waits on. The concrete class from 0x0015CD10, its parser at 0x0015D000 and its seven kinds of item are not recovered. The clock is a platform seam; the manager it sets the override of is `AtHeapManager`.
* `gm/settings` (8 functions): `app.ini`, the first file the game reads, what it sets, and the command line read after it. One original bug is held in a variable, not reproduced: see `s_levelUppercaseCount`.
* `at/gametext` (39 functions): text strings of both kinds and the `.AIS` table. Every game text string on the disc reads the same as the original's.
* `gm/names` (34 functions): the name-to-object hash table used across the engine, and the namespace tree above it. `gm/rumble` (21 functions): the reader of `RUMBLE.RMB`. What plays the effects is not recovered.
* `gm/levels` (11 functions), **partial**: the reader of `Levels.txt`, the first recovered code to give game data a meaning. **Recovered source now goes from the archive on disc to the game's level table**, on x64. Not recovered: the rest of its class, which
  owns level content (constructor 0x0013E150, destructor 0x0013E930, 0x0013E530), and everything that uses the table. `ReleaseLevelData` stops with a diagnostic, so unloading cannot yet work in a port.

| Measure | Result |
| --- | --- |
| Relocation-aware match, host compiler only | see docs/PROGRESS.md |
| Vftables matching by symbol | see docs/PROGRESS.md |
| Match with original compiler | 0 (blocked) |
| Behaviour vs. original code | see "Differential oracle" in docs/PROGRESS.md |

State of each function: **Semantic-tested** (differential oracle plus host tests); those with a MATCH line in docs/PROGRESS.md
are also a relocation-aware match under MSVC 19.44, which does **not** show which compiler shipped the game. The freeing path of
deleting destructors is not run by the oracle. The game itself cannot run from this source: this reaches its files, its text and its list of levels, and nothing yet loads a level.

The JAM directory layout is also recovered as a format (docs/formats.md, tools/jam.py), validated against all 56 archives.

## Confirmed findings

* Engine class families from RTTI: `At*` (application framework: `AtApBaseApplication`, `AtApGraphicsApplication`,
  `AtApScriptApplication`, … `AtApAtlasApplication`, `AtEv*` events), `Gm*` (core/game objects), `Fa*` (game layer:
  `FaAcPlayer`, `FaAcNPC`, `FaCmWeapon`, `FaUiBase`, `FaMoGameFramework`, …), plus `GmApp`/`App`. 118 type descriptors,
  67 vftables with RTTI locators. Error strings name more: `AtIoJamFileSystem`, `AtIoJamInfoHeader`,
  `AtIoFastAlignedJamHeader`, `AtIoFastJamHeader`.
* Socket/plug model: a connection links one socket and one plug and sits on both ends' lists, so destroying either end
  severs it. List nodes come only from a static pool of 4000 at 0x002E26F0. `GmLinkedList::Append` does not check for
  exhaustion. Items derive from `GmPlug` at offset 0.
* Methods first attributed to the template belong to base classes: `Contains`/`Remove` are `GmSocket`'s (vftable
  0x00278418), `Append`/`IsEmpty` are `GmLinkedList`'s (0x00278568). `GmLinkedListOf<T>` adds no vftable entries.
* Answered from cycle 1: the iterator destructor shows two vptr stores for three levels because `~GmSocketIterator` is
  implicit and its store is dead. The vftable 0x0027842C with no locator is `GmIterator`.
* 3,152 inventory entries are compiler-generated EH code, classified from `FuncInfo` metadata, and are excluded from
  function-count denominators. No `catch` block was found in any FuncInfo.
* Linker identical-code folding is in use (0x000DF720 and others), so one address can serve several source functions.
* Translation units can be inferred from address clusters and adjacent vftables: 0x00075FB0-0x0007606E (socket and
  iterator bases), 0x00076C10-0x000770FE (plug, list, list iterator; shares a unit with the JAM-opening code at
  0x00076640-0x00076BF0 and a sorted container at 0x000761B0-0x00076600), 0x0007CDF0-0x0007CEE0 (connection).
* Global `operator delete` at 0x00011210 routes through a heap object at 0x002C60AC and falls back to 0x001256A3
  (likely CRT `free`, unverified). 0x00125F25 is `__purecall` (raises runtime error 0x19).
* Entry 0x00085375 is XAPI startup (computes TLS size, creates the main thread at 0x00085301): SDK code.
* Coarse `.text` ownership from string references (medium confidence, gaps unknown): game-referencing functions
  in 0x00014250–0x0011FF80 and 0x00139F10–0x00192200 (and a few at 0x00200F20–0x00203A20); CRT strings around
  0x00127958–0x0012DE9A; CRI middleware 0x001B9A60–0x001F2000. Raw data: analysis-local/inv/string_tags.json.

* Text formats share one tokenizer (docs/formats.md). `AtIoTokenFile` opens the file; a format's reader passes a keyword table and pulls tokens. 0x00074800 (rumble) is the
  shape: open with extension `RMB`, pass the table at 0x002B2940, read blocks.
* `Levels.txt` lists 47 entries in order: 16 movies, 15 normal, 13 non-selectable, 3 hidden. Read by recovered code and checked against a separate Python parse (docs/formats.md). That is a checked starting inventory for M5; it does not show that each entry works.
* The tokenizer also writes. What the game writes as text is not established.
* 80 functions open text files through the tokenizer, each passing its own extension, many under 200 bytes (see the worklog for cycle 9). They are the natural list of format readers.
* The original's numeric parser is not correctly rounded, so its results must be reproduced, not improved (docs/formats.md).

## Open questions / hypotheses

* Original compiler = VC++ 7.1 with XDK 5849 (hypothesis; docs/toolchain.md).
* Core units apparently built with `/GR-` and game units with `/GR` (inferred from locators; docs/toolchain.md).
* JAM: meaning of info header +0x04; what the two JAM2 record sizes mean (Open bounds reads by the first, GetFileInfo reports the
  second); FSTA alignment rule; the unread byte in file entries; `AtIoFile` slot 8, whose argument count is inferred.
  `FSTR` is implemented from code and has never met a real file.
* `GmSocket::m_context`: role unknown. `GmConnection`, `GmLinkedListNode` and all method names are inferred.
* Static initialisers (0x00200ED0, 0x00203CD0) are compiler-generated and not recorded; `GmLinkedListIteratorOf<T>`
  (vftable 0x0027BCF0) is not recorded.
* DOLBY and PSFD00 sections: no x86 reached; contents unknown (possibly non-x86 code or data).
* Inventory: 14,370 `source` candidates are still candidates. Open disagreements with Ghidra are listed in
  docs/verification.md; 52 bodies contain another function's start.
* Ownership: tools/ownership.py sorts the code with labelled evidence (docs/PROGRESS.md). Most of the game row is inferred from neighbours, so its size is an estimate.

## Next concrete task

1. Integrate branches waiting: ASM/ASG, then the third batch (particle system actor, ASE, ASB, AGD, ADR, `.ANA` and the core every loader reaches).
2. `LIBRARY.ADD` (0x0009BD50), ASD 0x0002F7F0, AFD 0x00011770, the remaining node classes, image and font bodies (0x000586A0, 0x000415A0), the factory 0x000E9E90, the concrete database loader at vftable 0x0028A7D0.
3. Whether `AtSceneDevice`, `AtMaterialOwner` and the class in 0x00027B70-0x0002AC10 are one class, and whether `AtNameTableOwner` is really the class of vftable 0x00274A78.

Waiting on the owner, does not block the above: an authorised original-era MSVC to unblock M2.
