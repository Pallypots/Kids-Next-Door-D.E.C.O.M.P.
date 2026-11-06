# STATE — read this first (with CLAUDE.md), then `git status`, build/verify-report.json, build/oracle-report.json

Last updated: 2026-11-05, dev cycle 5 (archive file system; first end-to-end read). Per-function results live in docs/PROGRESS.md (generated).

## Milestones

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 Identity | **passed** | docs/provenance.md: input hashes, both containers parsed, identical XBE `f5f48a6f…`, retail key, 17/17 section digests, XDK 5849 |
| M1 Trustworthy tools | **partial** | inventory (tools/funcs.py), relocation-aware and vftable checker with negative controls, differential oracle qualified with source mutants, Ghidra cross-check done (docs/verification.md); **original toolchain blocked** |
| M2 Reference reconstruction | blocked | needs an original-era MSVC (docs/toolchain.md) |
| M3 Verified subsystem | **partial** | `gm/containers` and the archive layer recovered with type documentation, host tests and behaviour evidence against the original code; no original-compiler match, so "matching evidence" stays open |
| M4–M7 | untested | — |

## What is recovered

All source is in `src/`, types in `include/`. 148 functions and 19 vftables are recorded in `config/functions.toml`.

* `gm/containers` (47 functions): socket/plug connection model, connection base, pooled linked-list node, linked list,
  iterators, one template instantiation. Complete for these classes apart from the freeing path noted below.
* `at/jam` (101 functions), **partial**: info header, name table, directory lookups, header and file classes, `AtIoPath`, the base
  `AtIoFileSystem`, the file system stack of `AtIoContext`, and `AtIoJamFileSystem`. **Recovered source reads every file of every archive on the
  disc** on a 64-bit host, over a host file layer in `platform/`, which is port code. **Not recovered:** directory listing (`AtIoDirectoryListing`,
  the `List` and `FreeList` slots), the JAM1 class (no archive on this disc uses it), the rest of `AtIoContext`, the Xbox device file system, and
  the heap and error helpers. Unrecovered slots stop with a diagnostic in `src/at/AtIoJamPending.cpp`, which is never recorded and never counts;
  their vftable slots are checked by position only. `platform/fatal.cpp` stands in for the helpers.

| Measure | Result |
| --- | --- |
| Relocation-aware match, host compiler only | see docs/PROGRESS.md (for `at/jam`, 13 small functions) |
| Vftables matching by symbol | see docs/PROGRESS.md |
| Match with original compiler | 0 (blocked) |
| Behaviour vs. original code | see "Differential oracle" in docs/PROGRESS.md |

State of each function: **Semantic-tested** (differential oracle plus host tests); those with a MATCH line in docs/PROGRESS.md
are also a relocation-aware match under MSVC 19.44, which does **not** show which compiler shipped the game. The freeing path of
deleting destructors is not run by the oracle. The game itself cannot run from this source: this is the layer that loads its files, and nothing yet reads what they contain.

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
* Ownership of `.text` is not classified. The plugin's name hints could help here, but only as labelled hints.

## Next concrete task

The file layer now reaches real data, so what matters most is what the data means and what uses it.

1. Follow the code around 0x00076640-0x00076BF0 (the owner of the JAM path strings; `AtApDataManagementApplication` is a candidate, unverified) to
   how archives get mounted, and from there choose one archived file type that small, understood code consumes.
2. Library separation, now that there is enough code to tell engine from library: the denominators are still unclassified.
3. `AtIoDirectoryListing` and the `List` slots, and the sorted container at 0x000761B0-0x00076600, when something needs them.

Waiting on the owner, does not block the above: an authorised original-era MSVC to unblock M2.
