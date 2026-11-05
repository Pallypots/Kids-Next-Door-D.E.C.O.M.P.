# STATE — read this first (with CLAUDE.md), then `git status` and build/verify-report.json

Last updated: 2026-11-05, end of dev cycle 1. Measured numbers live in docs/PROGRESS.md (generated).

## Milestones

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 Identity | **passed** | docs/provenance.md: input hashes, both containers parsed, identical XBE `f5f48a6f…`, retail key, 17/17 section digests, XDK 5849 |
| M1 Trustworthy tools | **partial** | inventory (tools/funcs.py), relocation-aware checker qualified with 17 controls, 6 host-compiler matches; **original toolchain blocked** |
| M2 Reference reconstruction | blocked | needs an original-era MSVC (docs/toolchain.md) |
| M3–M7 | untested | — |

## Function states (reference build knd-xbox-usa-5454008a-v1)

| VA | Function (names inferred, class from RTTI) | State | Result with msvc14-host |
| --- | --- | --- | --- |
| 0x00076EC0 | GmLinkedListIterator::Next | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00076ED0 | GmLinkedListIterator::Prev | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00076F20 | GmLinkedListIterator::Current | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00076E80 | GmLinkedListIterator::Count | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00075FE0 | GmLinkedListOf<FaUiBase*>::Contains (REL32 call) | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00075FF0 | GmLinkedListOf<FaUiBase*>::Remove (REL32 call) | Relocation-aware match (host compiler) + semantic-tested | MATCH |
| 0x00076EA0 | GmLinkedListIterator::First | Draft source + semantic-tested | MISMATCH (register choice) |
| 0x00076EB0 | GmLinkedListIterator::Last | Draft source + semantic-tested | MISMATCH (register choice) |
| 0x00076F30 | GmLinkedListIterator::NextData | Draft source + semantic-tested | SIZE (19.44 copies `this` to EDX) |
| 0x00076F50 | GmLinkedListIterator::PrevData | Draft source + semantic-tested | SIZE (same) |
| 0x00076EE0 | GmLinkedListIterator::At | Draft source + semantic-tested | MISMATCH (store scheduling) |
| 0x00076E40 | GmLinkedListOf<FaUiBase*>::IsEmpty | Draft source + semantic-tested | SIZE (cmp vs test idiom) |

"Match (host compiler)" means the recovered source reproduces the original bytes under MSVC 19.44 with every
relocation resolved by symbol. It does **not** show which compiler shipped the game. Semantic tests use test-only
stand-ins for unrecovered callees (`tests/native/`).

## Confirmed findings

* Engine class families from RTTI: `At*` (application framework: `AtApBaseApplication`, `AtApGraphicsApplication`,
  `AtApScriptApplication`, … `AtApAtlasApplication`, `AtEv*` events), `Gm*` (core/game objects, containers:
  `GmSocket`/`GmPlug` connection model, `GmLinkedListOf<T>`), `Fa*` (game layer: `FaAcPlayer`, `FaAcNPC`,
  `FaCmWeapon`, `FaUiBase`, `FaMoGameFramework`, …), plus `GmApp`/`App`. 118 type descriptors, 67 vtables.
* `GmLinkedListOf<FaUiBase*>` : `GmLinkedList` : `GmSocket`, plus `GmPlug` at +0x10 (multiple inheritance).
  Iterator chain `GmLinkedListIteratorOf<T>` : `GmLinkedListIterator` : `GmSocketIterator` : `GmIterator`.
* Global `operator delete` at 0x00011210 routes through a heap object at 0x002C60AC and falls back to 0x001256A3
  (likely CRT `free`, unverified).
* Entry 0x00085375 is XAPI startup (computes TLS size, creates the main thread at 0x00085301): SDK code.
* Coarse `.text` ownership from string references (medium confidence, gaps unknown): game-referencing functions
  in 0x00014250–0x0011FF80 and 0x00139F10–0x00192200 (and a few at 0x00200F20–0x00203A20); CRT strings around
  0x00127958–0x0012DE9A; CRI middleware 0x001B9A60–0x001F2000. Raw data: analysis-local/inv/string_tags.json.

## Open questions / hypotheses

* Original compiler = VC++ 7.1 with XDK 5849 (hypothesis; docs/toolchain.md).
* Why the iterator's destructor chain sets only two vptrs for a three-level hierarchy (0x00137C80 → 0x00076040);
  vtable 0x0027842C has no RTTI COL in front of it. Possibly a compiler elision or a base compiled without `/GR`.
* Owner class of 0x00076CB0 / 0x00076C80 (named `FaUiBase::IsMemberOf/RemoveFrom` provisionally — likely belongs
  to a plug/member base class of FaUiBase).
* DOLBY and PSFD00 sections: no x86 reached; contents unknown (possibly non-x86 code or data).
* Inventory: 453 bodies contain another function's start (shared tails / bad seeds); pointer and gap seeds are
  medium/low confidence. Not yet cross-checked against Ghidra.

## Next concrete task

1. Install Ghidra + XboxDev ghidra-xbe (owner approval needed to download) and cross-check entry point, section
   map, kernel thunks and a sample of function boundaries against tools/funcs.py.
2. Recover the rest of the container family (Append 0x00077060, node allocation 0x00076F90, node init 0x0007CE70,
   plug membership 0x00076CB0/0x00076C80, destructors) — gives the first coherent subsystem.
3. Obtain an original-era MSVC (owner decision) to unblock M2.
