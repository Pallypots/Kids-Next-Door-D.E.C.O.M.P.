# 2026-11-08: reading the design data once loaded (subsystem `at/datalookup`)

Reference build knd-xbox-usa-5454008a-v1. State of everything here: Semantic-tested. Host compiler only; no byte-match claim (docs/toolchain.md).

The subsystem string is `at/datalookup`, not `at/adrread`: `at/adr` would be a prefix of the latter and claim its functions.

## Result

* `NO DIFFERENCE OBSERVED: 103 .ADR files in 32 archives read, and start-up with GENERIC.ADR and 11 under it; 111 registries copied; 1043287 lookups, 50155 values through the slots; 14672000 comparisons, 0 differences`. Entries reached: 61 / 61. Positive control fails as it should. Clean run: about 17 seconds.
* Every other suite, run with no mutants after the shared header edits: no difference, every entry reached.
* `test_datalookup` passes on x64; `test_adr` and `test_add` still pass.
* `tools/verify.py --only` on the 61 functions and 12 vftables: 5 MATCH, 10 MISMATCH, 46 SIZE for the functions under MSVC 19.44; all 12 vftables MATCH. No MISSING or UNRESOLVED. All twelve value classes' vftables now MATCH, so they are recorded as `[[vtable]]` in `config/functions.d/datalookup.toml` (their addresses stay in at/adr's `[symbols]` too, consistently).
* Closure, by `analysis-local/scratch/adrread/closure.py` (worktree), the fatal path counted as done: 61 functions, all recorded here; what is left is the C library only (`_stricmp` 0x00126F41 and the `tolower` under it, `memmove` 0x001262C0).

## Recovered (61 functions), names inferred

`src/at/AtDataLookup.cpp`, declared in `include/at/AtDataRegistry.h`:

* Lookups: `AtDataRegistry::Find` 0x000679E0 (the search every game object's record reader calls), `FindLink` 0x000678A0, `FindInPools` 0x00067950, `ValueNamed` 0x00067CB0, `ValueAt` 0x00067CF0, `FindPath` 0x00067D20, `ValueAtPath` 0x00068860; `AtDataLink::AppendPath` 0x0006C080.
* The other loading form, a registry made as a copy of another: `AtDataRegistry::CopyFrom` 0x000687D0, `AtDataComponentPool::CopyFrom` 0x00067200, `AtDataComponentPool::Relink` 0x00068720, `AtDataDescriptor::CopyFrom` 0x0006BCE0.
* Start-up's calls (0x0008DEA0): `AtDataRegistry::AttachUnder` 0x00067D40, `Create` 0x000675B0 (owner, twelve counts, parent; `ret 0x38`), `SetFileName` 0x00066860.

`src/at/AtDataValueAccess.cpp`:

* Slots 4 to 9 of the base value class: the shared stubs 0x000B15B0 (answers 0: Resize, ElementAt, Equals), 0x001ACD70 (SetElement), 0x000CBF30 (RemoveAt), and `Write` 0x00069E40.
* Slots 5 to 9 of the eleven typed classes, folded where the original folds: ElementAt 0x0006A010 0x0006A830 0x0006A9E0 0x0006AE50 0x0006B0F0; SetElement 0x0006A000 0x0006A020 0x0006A790 0x0006A760 0x0006A7B0 0x0006A7D0; Equals 0x0006A0E0 0x0006A130 0x0006A540 (0x0006A0A0 was already at/add's); RemoveAt 0x0006ADE0 0x0006AC00 0x0006B080 0x0006B6A0; Write 0x00069EB0 0x00069F00 0x00069FA0 0x0006A5C0 0x0006A970 0x0006ABB0 0x0006AD90 0x0006B030 0x0006B5C0.
* What the copy form calls on values: `Set` 0x0006A060 (Boolean, Int, Real, folded) and 0x0006A100 (ShortName); `AtDataString::CopyText` 0x0006A250; `Copy` 0x0006AD70 0x0006AB90 0x0006B010 0x0006B9E0; `SetData` 0x0006AC80 0x0006AAA0 0x0006AF10; `AtDataStringArray::Make` 0x0006B330; `AtText::CopyFrom` 0x00039910 (declared in `include/at/AtText.h`).
* `AtDataString` 0x0006A1D0 (`Forget`), 0x0006A4D0 (`BindAndRead`), 0x00069FD0 (`Bind`).

The stubs that stopped with a diagnostic in `src/at/AtDataComponent.cpp` are deleted; nothing in the value classes stops any more.

## What a real build can now read from real design data

With the oracle's evidence on all 103 `.ADR` files and on start-up as the game does it: load GENERIC.ADR, make the empty registry everything hangs under (`Create`), hang GENERIC under it (`AttachUnder`), name it `Library` (`SetFileName`), and load LIBRARY.ADD's eleven registries under it. Any link of any of those trees can then be found by its path from its own registry or from the top, climbing to parents as the record reader does, or by its name in the tree or in the blocks; and every value can be read (`ElementAt`), written out as text (`Write`), compared (`Equals`), set (`SetElement`) and shortened (`RemoveAt`) through the recovered slots, giving what the original gives. A registry can be copied by the copy form. Still missing for a game object to use this: the record readers that call `Find` (0x00093xxx, 0x0014xxxx and on: not recovered), the owner's own constructor, and a recovered caller of start-up (0x0008DEA0). The .ADB tables still reach the three `AtDataString` members through at/add's seams (see Requests).

## Quirks of the original, kept

* `Find` with climbing: a part found nowhere up the chain leaves null, and the next part is then looked for from the root, so `Zip\Actor` finds `Actor`. A null or empty path answers `from` unchanged.
* `FindLink` goes depth first through children and then siblings, so from a registry's root it runs on into the registries hung beside it.
* `ValueNamed` and `FindInPools` stop at the first block that answers; the exact flag is lost past a chained block (FindByName's quirk, as before).
* The copy form copies only each pool's first block; it hangs every link from the copy's root and then sets each one's `m_parent` by the other's parent's path (`Relink`), without moving it in the lists. Only parents one part deep are found, since nothing else hangs from the root. Destroying such a copy reads parents in link blocks already freed (Detach 0x0006C030): a use after free in the original.
* `AtDataDescriptor::CopyFrom` allocates the format one byte short, like `SetFormat`.
* `AtDataString::CopyText` always makes the copy wide. `Equals` on strings compares what slot 5 answers, which is null: different lengths are unequal, two empty texts equal, two others of one length crash.
* `AtDataStringArray::SetElement` writes the text's bytes, unterminated, over the array of pointers from the index on, not into any text. `RemoveAt` does not free the removed text's characters. `Copy` reads the first text's room even when there are none (null), and goes through a block on the token file heap; `Make`'s blocks are lost when `SetText` makes new ones.
* RemoveAt of any array resizes to one fewer whatever the index, and on an empty array asks for 0xFFFFFFFF elements.
* `Create` leaves the thirteenth block (no type) as it was, and does not set the count of types.
* `Write` of a one-value class writes the value, then a space and `WillAcceptParentData` if the flag is set; arrays write each element followed by a space.

## Oracle (`tests/oracle/oracle_datalookup.cpp`, suite `datalookup`)

Both sides are whole: the registry, its pools and the value classes are each side's own, loaded by each side's own loader. The comparison code is copied from the adr oracle.

* Real files: every archive's `.ADR` files loaded each under the one before; then for every link of every registry: Find from the root, climbing and not, with a trailing backslash, in lower case, from the link, from nowhere (climbing to the top then from the root), from the registry at the top of its tree; FindPath, ValueAtPath, ValueAt; FindLink by name, exact, under the parent, under itself (every link in the real-file runs); FindInPools and ValueNamed; AppendPath, with and without skipping, onto an existing text. Every value through ElementAt (first and last), Write (a logging writer: each Put with its argument, strings by hash and length), Equals with itself and with the value before it. Then each registry copied by the copy form, compared whole, looked up again, and every value changed through SetElement and RemoveAt and compared again.
* Start-up, as above, with every lookup and value read on all thirteen registries, then a registry made with counts attached by a real path, a missing one, an empty one and null.
* Written text: every type including ByteArray, flags, -0.0, equal and unequal neighbours; a tree with a backslash in a name, an empty name, no name (which crashes the path search on both sides, compared as a crash), names alike but for case, a chained block; paths no link has; copies over copies.
* Direct: every slot of every class on hand-made values with and without the flag; reals' zeroes and NaN; short names equal to the terminator; strings of different and equal length (the crash); SetElement for each index and count; RemoveAt down to none; Set, CopyText, Copy and SetData twice over (over held data), from nothing, Make, Copy of no texts (crash on both); `AtText::CopyFrom` for each kind into each kind, at and past the room, from a start past 0; Bind, BindAndRead, Forget over nothing, narrow and wide text; descriptor copies typed and not, with and without formats; no heap and no memory for each allocating function, and Make failing at each of its requests; AppendPath on hand-built links; Create in each form and SetFileName through each length.

What a green result does not show:

* RemoveAt on an empty array is not run: it asks Resize for 0xFFFFFFFF elements, whose byte count wraps, and the harness's padding wraps it again into a small block both sides overrun.
* While a registry is destroyed, freed blocks are not given back on either side (`g_keep_freed`), so the copy form's use after free reads the same thing on both sides instead of whatever each host heap reused.
* Lookup answers are compared as places in a walk of the registries; an answer outside them (stale) compares only as "elsewhere".
* The writer is the harness's; the real text tokenizer's output of these values is not compared, only what is asked of it.
* No game code above these runs: the record readers are not recovered.

## Mutants

273 / 273 detected. Of 282 first written, one did not compile (my typo, fixed) and the first full run left 28 alive. Eighteen were gaps, closed in the harness: a nameless link under a nameless link (AppendPath's skip), FindPath climbing, FindLink from GENERIC's root reaching the library's roots hung after it, a signalling NaN as a bound, the copy form and Create into a registry not loaded whose blocks hold leftovers (used, values, next, parent), a pool heap left different before a copy, the token file's heap given its own token, attaching by a path only climbing finds, booleans that were never clamped, a string flagged wide that holds nothing, a text array's text starting past 0. Ten were equivalent: nine dropped, each with its reason at the head of `tests/oracle/suites/datalookup.py`, and one ("find: empty path", which returned null where the original returns a null `from`) replaced by "find: empty path answers null", which changes behaviour and is detected. The second full run left one: a mutant the compiler folded (`x * 1.0f`); rewritten as `x + 0.0f` it is detected, in a run of that mutant alone.

## Requests for the integrator

* Replace at/add's three seams with the members: `AtDataStringReset(v)` becomes `v->Forget()`, `AtDataStringBindAndRead(v, d, t, c, n)` becomes `v->BindAndRead(d, t, c, n)`, `AtDataStringBind(v, d, c, n)` becomes `v->Bind(d, c, n)` in `src/at/AtDatabaseManager.cpp`; then delete them from `include/at/AtDataLibrary.h`, `src/at/AtDataLibraryHost.cpp`, `tests/native/test_add.cpp` and `tests/oracle/oracle_add.cpp` (whose doubles call the original's code for them). Not done here: those files are at/add's.
* at/add's oracle runs FindByValue only on Boolean and Int columns because slot 7 of the others stopped; it no longer does.
* at/adr's `config/functions.d/adr.toml` comment says the value vftables are not recorded since slots 5 to 9 are not recovered; they now are, here.
* `docs/formats.md`: the registry's lookups and copy form above.

## Shared edits

* `include/at/AtDataRegistry.h`: additive declarations for every member above, and the slot comments of `AtDataComponent` rewritten (they said the slots stop, and that Write ends a line; it writes a space and a word). Nothing renamed or retyped.
* `include/at/AtText.h`: `AtText::CopyFrom` declared.
* `src/at/AtDataComponent.cpp`: the six stub definitions and their helper deleted (at/adr's file; its mutant texts do not touch them).

## Mistakes of my own

* The first oracle runs crashed nondeterministically while destroying copies; it was the original's use after free above, read through heaps that differ by side, not a difference. Fixed in the harness, as described.
* My first host test expected FindLink from a link not to reach that link's siblings; it does.
