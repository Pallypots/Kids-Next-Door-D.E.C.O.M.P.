# Verification

## States (CLAUDE.md §7)

Unanalyzed → Analyzed → Draft source → Semantic-tested → Relocation-aware match → Exact linked match →
Native-integrated. A state is claimed only with the command that proves it.

## The checker: tools/xcheck.py

Compares one function of an MSVC i386 COFF object with the reference XBE:

1. Candidate = the function's COMDAT section (`/Gy`) or symbol-to-next-symbol bytes.
2. **Length must equal** the recorded original size (catches removed or extra code, e.g. different idioms).
3. Every non-relocated byte must be identical.
4. Every relocation resolves **by symbol name** through `config/functions.toml [symbols]` (plus the recorded
   functions themselves). DIR32: original u32 == symbol + addend. REL32: field+4+rel == symbol + addend.
   An unknown symbol → `UNRESOLVED`; unsupported relocation type → `UNSUPPORTED`. Nothing is masked.

Results: `MATCH`, `MISMATCH`, `SIZE`, `UNRESOLVED`, `UNSUPPORTED`, `MISSING`, `COMPILE_ERROR`.

Vftables are checked too (`check_vtable`, `[[vtable]]` records): the slot count must be exact and every slot must name
a symbol whose mapped address equals the original slot. A relocation through a weak external (modern MSVC names
`??_E...`, a weak alias of `??_G...`) resolves to its default definition, as a linker would. This establishes class
layout and slot order for the recorded classes.

Scope limits (known, deliberate): does not verify jump tables in data, static initialisers or linker layout; those need
their own checks before any whole-XBE claim. Identical-code folding means one address can serve several source functions,
so a symbol mapping asserts "calls this address", not "this is the only function here".

## Qualification — `python -I -m unittest discover -s tests -v`

69 tests, all passing on 2026-11-05:

* `tests/test_parsers.py` (12, synthetic): XDVDFS listing, case-insensitive lookup, explicit base, bad magic,
  truncated file, directory-tree cycle; XBE retail/debug keys, zero-fill not file-backed, entry outside code
  rejected, bad section digest reported, raw>virtual rejected, bad magic/truncation.
* `tests/test_jam.py` (18, synthetic): FSTA, FSTR and JAM2 parsing; bad token, JAM1, truncation, oversized header, child
  table outside header, directory cycle, bad name and extension index; extent, overlap, inside-header and misaligned
  record detection.
* `tests/test_funcs.py` (6 synthetic, 4 local): FuncInfo parsing, null actions, bad magic, implausible counts, thunk
  scan needs the magic, kind precedence; on the real inventory: a tail call is not absorbed, known funclet and thunk are
  labelled, recovered functions stay `source`.
* `tests/test_xcheck.py::CompareCore` (10, synthetic): exact match; wrong constant; reversed condition; REL32
  correct; REL32 wrong target; DIR32 data reference right/wrong; DIR32 addend respected; unknown symbol →
  UNRESOLVED; unsupported reloc; truncated and padded candidates → SIZE.
* `tests/test_xcheck.py::VtableCore` (6, synthetic): match, swapped slots, too few or too many slots, slot without a
  relocation, unknown symbol, wrong relocation type.
* `tests/test_xcheck.py::CoffParsing` (3, synthetic): `.bss` size is not a file extent (regression), data outside the
  file is rejected, weak externals resolve to their default.
* `tests/test_xcheck.py::RealObjects` (10, local only — needs the owner's XBE and MSVC): real compiled variants:
  positive control `Next` → MATCH; `->prev` instead of `->next` → MISMATCH; reversed condition → not MATCH;
  correct call → MATCH and the same call with the symbol mapped to the sibling function → MISMATCH; call with an
  empty symbol map → UNRESOLVED; function absent from the object → MISSING; correct source checked at the
  neighbouring address → MISMATCH; vftable positive control, vftable at the wrong address, vftable with slots in the
  wrong order.

Any checker change must keep all of these passing and add a regression test for the change. Rerun
`tools/verify.py` after any checker or toolchain change.

## Running

```
python -I tools/verify.py            # compile config'd sources, check every function, write build/verify-report.json
python -I tools/xdiff.py <obj> <symbol> <va> --size N   # side-by-side diagnostics (no verdict)
python -I tools/report.py            # regenerate docs/PROGRESS.md from the measured data
```

The report embeds the XBE SHA-256, the checker SHA-256 and the compiler version line.

## Differential behaviour oracle: tools/oracle_run.py

Byte matching needs the original compiler. Behaviour can be checked without it. `tests/oracle/oracle_gm.cpp` is a 32-bit
Windows program that maps the owner's XBE at its original addresses, runs the **original machine code** beside the
**recovered source**, feeds both the same pseudo-random operations and compares return values and the whole object
graph (every list, plug and in-use pool entry, identities compared by index) after each operation. Other functions
are reached through the original vftables, so slot behaviour is exercised as well.

This is analysis tooling, permitted by the charter as a behaviour oracle. It is never part of a release, contains no
original bytes (they come from the owner's file at run time) and needs no emulator. It runs only code that touches no
Xbox hardware or kernel, so it does not extend to most of the game without further work. How it gets the address range:
linked at base 0x10000 with `tests/oracle/reserve.asm` as the first code in the image, so the XBE's whole range, from 0x00011000
up, belongs to the image from process creation (incremental linking must stay off: its thunks would land first).

Result state: **Semantic-tested**. A pass means no difference was observed on these sequences. It is not a proof and
not a byte match; paths not taken are untested. Known gaps: the freeing path of deleting destructors (needs the game's
heap) runs with flags 0 only; exception paths are not run; `Prev` on a null current and `At` on an empty list are avoided
because both sides crash.

Qualification, all run by `tools/oracle_run.py`:

* one-shot breakpoints prove every recorded function's entry is reached in every seed;
* `--mutate` (recovered side deliberately skips a call) must fail;
* an unmodified copy built through the mutant path must pass (positive control for the mutant build itself);
* source mutants, each one small edit to a copy of the recovered source, must all be detected.

Result on 2026-11-05 (`python -I tools/oracle_run.py --seeds 20 --ops 5000`): 20 seeds, no difference in about 405 million
comparisons; 47/47 entries reached in every seed; `--mutate` fails; positive control passes; 23/23 mutants detected (13 as
a reported difference, 10 by crash or hang, which also counts as visible). Numbers regenerate
into docs/PROGRESS.md.

A second suite covers the JAM reader (`tests/oracle/oracle_jam.cpp`, `--suite jam`; inputs from
`python -I tools/jam.py cases <image> analysis-local/oracle-jam`, git-ignored). It runs 62 archives: 35 real FSTA headers, 20 real
JAM2 files (those up to 4 MB), 4 FSTR forms derived from real headers (FSTR does not occur on this disc), and 3 hand-built
adversarial archives. Each real file is checked three ways: `tools/jam.py`, original code, recovered source. Near-miss paths
only need the two implementations to agree.

Object lifecycles run under each archive-file behaviour (immediate, pending, pending that fails, read failure, seek failure) and
with a heap that was never set: `vInit` twice, direct `Shutdown`, files opened until none are left, open with various flags and
paths, reads and seeks around every boundary, close, reopen, destruction through the deleting destructor. In the mapped copy of
the original, the heap and error helpers (0x000111F0, 0x00011210, 0x00011240, 0x00011260, 0x00011070, 0x00011170) are redirected
to logging stubs; this is test scaffolding, in memory only. Each side has its own test-double archive file. Their call logs
(helper calls, seeks, reads, waits, releases, in order) must be identical, and fatal messages must match as text.

The file system layer runs in the same suite: `AtIoPath` over 20,000 random operations, the `AtIoContext` stack including search order
across file systems holding the same file, the base file system's defaults, and `AtIoJamFileSystem` from `Create` through `vInit`, every
slot, a non-empty root, exhaustion, `Shutdown`, restart and destruction, over a test-double device.

Result on 2026-11-05: 174,373 paths, about 196 million comparisons, no difference; 101/101 entries reached; each of the 62 cases also
passes alone, so results do not depend on case order; positive control passes; `--mutate` fails; 136/136 source mutants detected (130 as
a reported difference, 6 by a crash).

A run first tries each mutant on a small case set and gives a survivor the full set, which changes run time and not the verdict.
`tools/oracle_run.py` refuses stale mutants before starting, and **aborts if the positive control fails**: see below.

Not covered by this suite: directory listing and the JAM1 class (not recovered); a missing archive, a device `Open` failure and an invalid
token, where the original dereferences null after reporting, so both sides crash if the error helper returns; allocation failure; paths
long enough to overflow the fixed buffers, in both implementations; the real Xbox device layer, which is not recovered; JAM2 archives over
4 MB (the limit is only about file size in the cases directory).

## Text tokenizer: tests/oracle/oracle_tok.cpp

Same method as the other suites, in lockstep: after every token, both sides' return value, token type, integer, real (as bits), string, identifier, line and
buffer positions are compared, along with calls to the file below and every message.

Input: every distinct file on the disc that begins as text, plus 60 binary samples (`python -I tools/jam.py textcases`); text written to reach each state and limit;
1,500 random strings; six read behaviours (normal, pending, one byte at a time, zero at end, failing, failing wait); every write method on its own with the fields
it sets made dirty first; 300,000 random floats and many small exact ties through the float formatter.

Result on 2026-11-05: 5,864 files, 12,749,375 bytes, 1,372,044 tokens, 22,236,514 comparisons, no difference; 99/99 entries reached; positive control passes; `--mutate`
fails; 172/172 source mutants detected (166 as a reported difference, 6 by a crash or hang).

The real rumble file was also read with the game's own keyword table from 0x002B2940: 145 keywords recognised, no difference.

`python -I tools/oracle_run.py` needs `analysis-local/oracle-tok`. Its first run was 159/172. The 13 survivors were: types checked only after a whole series of writes, so a
later write hid an earlier one (3); inputs short of a limit (5); a failure path never entered and a field not compared (2); state a normal sequence never leaves (2); and one branch of
my own that did nothing, now removed (1).

**x87 precision.** When this was written the Xbox's setting was not established, so each file was also tokenized by the original under 53-bit and 64-bit precision. No file's result changed. The setting on the game's main thread is since read from the binary: 53 bits, see docs/portability.md.

**On a 64-bit host.** `test_at_tok <cases> <digests>` tokenizes the same files in an x64 build (SSE, no x87) and compares with digests written from the original: 5,864 files,
1,355,279 tokens, no difference. The digest covers each token's values, floats as bits.

Differences from the original, each deliberate:

* After a syntax error on a character below `!` or above `~`, the original indexes its token table out of bounds and takes what it finds as the token (seen: `0x74636570`,
  which is "pect" from a nearby message). The recovered code stops. This is only reachable if the fatal error routine returns. On such a token only the first message is compared.
* With no file name, the original appends the message to an uninitialised buffer. The recovered code starts it empty. Only the end of that message is compared.

Not covered: the code that uses tokens, which is per format and not recovered; `Report`, whose result goes nowhere, so only reaching it is tested; allocation failure beyond the
missing-heap case; `PutReal` of values too long for its 0x20-byte buffer, which overflows in both.

## Levels.txt reader: tests/oracle/oracle_lvl.cpp

The original's `Load`, `Unload`, `Reset`, name array and name class beside the recovered ones. Tables, names, descriptions, heap calls, file calls and messages are compared after each step, on the real file and on 42
inputs written for each path, including a second block that grows or shrinks the table, wrong tokens, reloading over a table, and no heap. A failed open makes both sides dereference null after reporting; the harness
catches that, so the message can be compared.

Result on 2026-11-05: `Levels.txt`, 47 entries, 1,910 comparisons, no difference; 11/11 entries reached; positive control passes; `--mutate` fails; 60/60 source mutants detected (54 as a difference, 6 by a crash).

First run 59/61. One survivor was a real gap: the harness's allocator returned zeroes, which hid a missing store, and the game's heap is not cleared. This suite now fills allocations with 0xCD. The other, swapping two same-sized
allocations, changes only addresses, so that mutant is equivalent and was dropped.

Not covered: 0x0013E530, which releases level content and is not recovered (the harness replaces it on both sides); the constructor and destructor, for the same reason; a file ending inside a block, which hangs on both sides by reading
the code, and was not run.

**End to end on x64.** `test_gm_levels <dir> <expected>` mounts the real `AppInit.JAM` over host files and calls `Load`, so everything from the archive to the level table is recovered code. The expected list comes from `tools/levels.py`, a separate
parse in Python written from the text alone. 47 entries, no difference.

**A bug this found in the recovered code.** The original allocates the description table as `count * 4`. Copying that overflowed the table on x64, and every value still compared equal; the run only showed it as heap corruption at exit. The source now uses the element
size, which is unchanged on 32 bits. No 32-bit test could have seen this.

**AddressSanitizer** (x64 build, `/fsanitize=address`): all five native tests, the real `Levels.txt` run, and all 5,864 tokenizer files: no report. As a control, putting the bug back makes it report a heap overflow. Reads of `AtCopyName9` past a short name were not seen in these runs. This does not cover the oracles, which run original code.

## Mount manager: tests/oracle/oracle_mnt.cpp

Each side has its own device, root, factory and context; a synthetic FSTA archive goes through the original's and the recovered archive classes. Result on 2026-11-06: 946 comparisons, no difference; 23/23 entries; positive control passes; 64/64 mutants
(51 as a difference, 13 by a crash).

Its first run was 50/64, and it was partly hollow: three non-virtual methods were called as recovered code on both sides, so they compared with themselves. Entry coverage showed it: three entries unreached. They now call the original by address. The header's slots 4
and 5 are empty in the real classes, so the harness patches logging stubs into both vftables to show that they are called.

`test_gm_levels` now reaches the real archive through `AtIoMountManager::Init` and `Mount`, with a factory of host file systems, as start-up does.

Not covered: the real factory and device classes, which are not recovered; `AtIoSpaceManager`'s other methods at 0x0007CF00-0x0007D0E9.

## Name map and rumble reader: tests/oracle/oracle_rmb.cpp

The name map under 40 seeds of 1,500 random operations each, with the whole table compared after every one, plus sizes at and past the end of the prime table, group membership and no heap. The rumble reader on the real file and on 34 written inputs. Frames are compared as bits.

Result on 2026-11-06: `RUMBLE.RMB` with 15 effects, no difference; 36/36 entries; positive control passes; `--mutate` fails; 125/125 source mutants detected (103 as a difference, 22 by a crash or hang).

First run 88/95, then 121/129 with the namespaces added. Most survivors were real gaps, now closed; those that changed nothing observable were dropped, with the reason beside them in tools/oracle_run.py. One of the tests had been comparing no-ops: a group's name had been reset before its moves, on both sides, so nothing moved. It now asserts that they do.

Not compared: map slots when an effect's name failed to parse. The name is then empty with stack bytes after it, and the hash reads past the terminator, so the slot depends on those bytes in the original too.

`test_gm_levels` also reads the 15 effects and 38 key frames from the real archive on x64, against a separate Python parse, with floats within a tolerance because Python's are correctly rounded and the game's are not.

## Text strings and game text: tests/oracle/oracle_txt.cpp

Every string method through the vftables, on 21 texts, for both kinds, with windows that start later, plus 31 written tables and all 72 distinct `.AIS` files on the disc, each string looked up on both sides and compared. Result on 2026-11-06: 2,685 strings, no difference; 39/39 entries;
positive control passes; `--mutate` fails; 64/64 source mutants (54 as a difference, 10 by a crash). First run 54/66.

**Not run: the wide `ToFloat` at 0x0002DDC0.** It is `swscanf`, and the original's needs C library locks only start-up creates. It is not recorded, and is compared instead with the original's narrow `atof` on the same text. A wide string that parses to nothing returns an uninitialised local in the original, and zero here.

Library differences found: `atoi` wraps on overflow in the original and may saturate in a modern library, so the conversion is written out; the status text table has a gap at 19, which my first version missed. `Insert` turned out to take (character, index), not the reverse: the original crashed when the harness called it the other way, and I had read the stack offsets wrongly.

The harness now reserves up to 0x004EF000, because the C library touches uninitialised data past the old limit.

## App.ini reader: tests/oracle/oracle_set.cpp

Every key with good and bad values, the file on the disc, each debug channel on and off, each language and region, a missing file and one that will not open. Result on 2026-11-06: 8,637 comparisons, no difference; 7/7 entries; positive control passes; `--mutate` fails; 86/86 source mutants (81 as a difference, 5 by a crash). First run 82/87; the gaps were fields the zeroed test memory hid. One mutant was equivalent and dropped.

The language and region calls, 0x001B7B84 and 0x001B7B12, read the console's settings and are replaced on both sides in the harness.

`test_gm_levels` now reads the real `app.ini` first, as the game does. That run found a bug in the port's host file layer, which treated any short read as an error: the buffered stream asks for a whole buffer, and the archive code had never asked for more than there was. A short read with data in it now succeeds.

## Command line reader: tests/oracle/oracle_set.cpp

The same suite covers 0x00098410: every option alone and followed by another, case, numbers at and past their limits, each debug channel, values that are themselves option names, and the three options that crash when their value is missing. Result on 2026-11-06, with the `app.ini` cases: 16,705 comparisons, no difference, 8/8 entries, 140/140 mutants.

**Known deviation.** The original uppercases a level name for `strlen` of 128 stack bytes that nothing writes. Source cannot reproduce stack garbage, so the recovered function uses `GmAppSettings::s_levelUppercaseCount`, 0 by default. The harness fills the original's buffer, directly under its return address, to choose the count, and compares the result for counts from 0 to 127, including writes past the end of the name. What the stack holds on a real boot has not been measured, so the default of 0 is a choice, not a finding. With 0, only a level name already beginning with a capital `L` is accepted.

## End to end: tests/native/test_at_e2e.cpp

Opens real archives through the recovered layers (`AtIoContext`, `AtIoJamFileSystem`, the header and file classes) over host files, and
reads files out of them in pieces. Expected sizes and CRC-32s come from `python -I tools/jam.py e2e <image> <dir> --all`, which slices each
archive with Python alone. Result on 2026-11-05, x64: **all 56 archives, all 26,508 files, 919,875,457 bytes, no failure**. With no arguments it
uses a synthetic archive and needs no game data. `platform/host_file_system.cpp` beneath it is port code, not recovered code.

This shows the recovered path works on a 64-bit host. It does not show the host file layer behaves like the Xbox's, and it says nothing about
what the files contain.

The reader's mutant runs were not clean the first time. 30/40 on the lookups: eight were real blind spots, because real data never
shows them (labels are short, no real file lacks an extension, no table has an empty name, record sizes are always equal, small JAM2
trees never reach the failed-component behaviour, some fields already held the value being checked); the adversarial archives and
sharper checks close them. Two were equivalent mutants: `order >= 0` in the scan (order 0 has already returned) and not clearing
GetFullName's first two words (`strncpy` pads them), and were replaced. 81/85 on the file objects and lifecycles: `Shutdown`'s field
clearing is overwritten by its only caller, a `Clear` before every `Close` hid what `Close` does to the flags, and a destructor's
stores are visible only by inspecting the memory afterwards. Each now has a direct test.

File system round, 132/136 at first: the message check kept only the last message, so a later failure hid an earlier wrong one; no two test
devices held the same file, so search order was invisible; root was always empty, so full and relative paths were identical; and no root
reached SetRoot's limit. Each now has a test.

**A hollow result, caught by the positive control.** One run reported 136/136 with the control failing. A test I had just added left entries
in the call log, and the small case set's first archive tripped over them, so every mutant failed that one comparison whatever it did. The
count meant nothing. The tool used to record that and carry on; it now aborts, and all 62 cases are run singly as a check on ordering.

Harness bugs the lifecycle run found in itself: the recovered side's global `operator new` logged the harness's own allocations, and
storing a fatal message in a `std::string` allocated during logging. Both showed as extra entries in the call log, and the log
dump identified them; neither was a code difference.

Mistakes this qualification caught in the harness itself, recorded because they would have produced false confidence:
copied sources kept old timestamps, so the build reused the previous mutant's object and later verdicts were wrong
(sources are now rewritten fresh); `--mutate` triggered too rarely to ever fire; the list destructor's context clear was
invisible because the object was deleted before comparing. One candidate mutant (removing the `return` in
`GmPlug::Disconnect`) turned out to be equivalent, because `Unlink` zeroes `m_plugNext` and stops the loop anyway, and
was replaced. A surviving mutant must be explained like this, never just dropped.

## Independent cross-check: Ghidra (tools/ghidra_compare.py)

Ghidra 12.1.4 with ghidra-xbe, headless (`docs/toolchain.md` has the command; the export script is
`tools/ghidra/ExportInventory.java`; analysis took 147 s). Agreement between tools is corroboration, not proof, and neither is
the reference. Results on 2026-11-05 after the fixes below:

| Check | Result |
| --- | --- |
| Section map | 17/17 same address, size and execute flag |
| Entry point | 0x00085375 in both |
| Kernel import slots | 124/124 carry a Ghidra import symbol at the same slot |
| `.text` bytes reached as code | ours 91.8%, Ghidra 90.5% |
| Function starts | ours 17,522 (14,370 source, 3,152 compiler-generated); Ghidra 11,882; shared 11,352 |
| Extents of shared starts | same end 11,307; different 45 |
| Recorded functions | 47: 36 agree; 11 differ, all explained below |

The 11 recorded differences, checked against the disassembly: 4 are recorded as contiguous extents and include alignment
padding that Ghidra's flow analysis skips (`DisconnectAll`, `Disconnect`, `IsConnectedTo` 3 bytes; `Allocate` 6 bytes, Ghidra
shows two ranges). 7 are functions reached only through vftables, which Ghidra disassembled but did not make functions.
Those 7 are supported by the vftable checks and the oracle. Ghidra found no error in them.

Open disagreements, not resolved: about 1,830 pointer-seeded and 700 gap-seeded starts where Ghidra has code but no function, most
likely vftable-only or indirectly called functions, since Ghidra's RTTI analyzer targets PE files, though each one is
unverified; 197 source starts of ours where Ghidra has no code; 22 of those are `call` targets, reached from code that Ghidra did not
disassemble; 530 starts only Ghidra has (381 in library sections that funcs.py only partly explores); 45 differing extents,
some still absorbing a jump target not preceded by padding (0x000C93B0). **The 14,370 is therefore still a candidate count.**

What the cross-check corrected in tools/funcs.py:

* 3,152 "functions" were compiler-generated EH code: 1,957 unwind funclets and 1,195 handler thunks. Found because Ghidra
  did not disassemble the pointer seeds in 0x001F5xxx-0x00200xxx. Now classified from the `FuncInfo` metadata itself, not a
  heuristic, as `kind`. Every thunk's handler is 0x00125784, which the plugin independently names `___CxxFrameHandler`.
* Function extents depended on discovery order and absorbed tail calls (0x00013380 "ended" at 0x0001F2F4). Bodies containing
  another start fell from 453 to 52.

Denominator change, with the reason above: cycle 1's 15,544 candidates become 14,370 `source` candidates plus 3,152 generated.
New starts appear from the tail-call rule. Compare only like with like.

## Other checks in place

* Layout `static_assert`s for the original 32-bit offsets compile under x86 modern MSVC (proved active by a
  deliberately wrong offset that fails the build).
* Host semantic tests, against recovered code with no stand-ins: `cmake -S . -B build/native -A x64`,
  `cmake --build build/native --config Debug`, `ctest --test-dir build/native -C Debug`.

## Audio data readers: tests/oracle/oracle_aud.cpp

`python -I tools/oracle_run.py --suite aud` compares 0x00136DA0, 0x00136E70 and 0x001370E0 with the recovered source over the disc's two files and 44 written inputs, each read twice, once with every third memory request refused. Result: no difference, 3/3 entries, 53/53 mutants.

Limits: no real file exercises the block path, because the disc's `MEMBLOCK.GOB` has no `XBOX` section, so that path rests on written inputs only. The capacity of 100 blocks is inferred from field offsets; neither side checks it, and no test goes past it. The audio manager and its memory object are doubles.

## Time-sliced loader: tests/oracle/oracle_ldr.cpp

`python -I tools/oracle_run.py --suite ldr` compares the 10 base-class functions over the disc's 51 `.MNG` files and written cases: each state, clock steps against budgets, a clock that wraps, files that will not open, a null tokenizer with a file open, a stale link on the first item, destructors with a file open. Result on 2026-11-06: 19,502 comparisons, no difference, 10/10 entries, 56/56 mutants.

Limits: the class is abstract, so a double supplies its four virtual slots and its items on both sides, and this says nothing of the concrete loader. The clock is replaced on both sides. The original builds a name ending `.txt` on opening and never reads it; the recovered source does the same, and no test can see it. The vftable is checked by position; four slots are pure.

## Allocator: tests/oracle/oracle_hep.cpp

`python -I tools/oracle_run.py --suite hep` compares all 22 functions. Each side has its own arena with the same random bytes to begin with, at fixed addresses exactly 16 MB apart, so that alignment is alike and a pointer differs only in its top byte, wherever it lies. After each batch **the whole arena is compared byte by byte**, the only difference allowed being the two sides' top bytes: headers, links, tags, and what is left in freed memory. Parts: a lone heap at six alignments and nine sizes in every order of freeing, exact fits and refusals, widths around the heap's, markers with odd and far distances, each fatal, destroying in every order, a fixed heap nested in a carved one, equivalent groups spilling over with and without an override, each capacity of the report. Then 40 seeds of 3000 random operations. Result: no difference over 3,100,506 comparisons, 22/22 entries reached, 154/154 mutants detected, positive control fails as it should.

Five further mutants were dropped as equivalent, each with its reason beside the list in `tools/oracle_run.py`. Ten survivors of the first run were real gaps, each visible only in a state ordinary use does not reach; `run_states` sets those by hand on both sides.

Limits: fatals and `AtIoTokenFile::s_heap` are replaced or set by the harness. The 24-bit size limit is tested with written headers, not a real heap of that size. Nothing here runs the game's own sequence of allocations, whose set-up is not recovered. On a 64-bit host the header is larger, so `tests/native/test_at_heap.cpp` checks properties rather than bytes: no overlap, data kept, alignment, everything coming back.

## Memory system: tests/oracle/oracle_mem.cpp

`python -I tools/oracle_run.py --suite mem` compares the heap stack, new and delete, the error routine, both memory system classes and the saved globals, with the real recovered allocator beneath on one side and the original's on the other. The original's calls to allocate and free its region, to start devices and to the library's free are replaced and recorded with the arguments it passes, so `VirtualAlloc(0, size, 0x3000, 4)` and `VirtualFree(p, 0, 0x8000)` are checked on its side; the recovered side's seams take less, and record the same. Regions at fixed addresses are compared byte by byte, as in the allocator's suite. Cases: five sizes of region and heap, allocating from whichever heap is on top, starting twice, shutting down twice (the second frees the same pointer, as in the original), each pop condition, refused memory, a default heap too large, each error code, depth past the table, each path of restoring. Result: no difference over 9,715 comparisons, 19/19 entries, 61/61 mutants, positive control fails as it should.

Limits: unbalanced pops write outside the table, in the original and here, and are not run; what they would overwrite differs on a host. Deleting destructors run without the delete. Slot 0 of the base class is pure in the original, so a double supplies it on both sides. The device call's identity, `XInitDevices`, is a guess from its shape and its section.

Inventory: 0x001038C0 has no entry in `analysis-local/inv/functions.json`, because it is reached only through a thunk, so its recorded size, 25, is from the disassembly.

## Crash guard: tests/oracle/oracle_guard.h

Four suites and the memory suite run code that crashes on purpose, on both sides, since the original carries on past a report that does not stop in this harness. Up to cycle 14 `oracle_lvl`, `oracle_set` and `oracle_txt` resumed from `RtlCaptureContext`, which on x86 captures its caller's frame: a resume left the capturing function with callee-saved registers as they stood at the crash. Those suites had given no false results that are known of, but their comparisons after a crash rested on registers no one had restored. All now use one guard that saves and restores them, and their figures in docs/PROGRESS.md are from reruns with it.

## Vector math: tests/oracle/oracle_vec.cpp

`python -I tools/oracle_run.py --suite vec` compares 14 functions by bits at x87 precision 53, over 200,000 rounds of ordinary and edge values, 50,000 with infinities and NaNs, and 1,000,000 built to land next to a float midpoint, where a sum's order can show. Result: no difference over 44,000,024 comparisons, 14/14 entries, 41/41 mutants, positive control fails as it should.

`tests/native/test_at_vector.cpp` holds the x64 build to a hash of the original's own results, printed by the oracle, for the 11 functions with no sine or cosine: **the hash matches, 0x7B61BE23F1D29A03**, so on those inputs SSE double arithmetic gives the original's bits, NaNs counted as alike.

**Eight mutants survive and are not counted.** Each changes only the order of a sum, or `(1 / length) * x` to `x / length`: `Dist2: order`, `Dist: order`, `Normalize: direct`, `Rotate: y order`, `Rotate: z order`, `Rotated: z order`, `Project: order`, `SetProjected: order`. They are not equivalents: such a change moves a double by its last bit, which reaches a float only next to a midpoint, about once in 2^29 random inputs, and the midpoint inputs did not reach these. So the source's order for those sums rests on reading the disassembly, not on this test. Effect if one were wrong: a last-bit difference in one float, on rare input.

Limits: NaNs that differ only in sign count as alike, and 10,522 such are counted and printed: that sign depends on operand order and x87 rules, so it is not claimed. Signalling NaNs are not generated. The functions return a float in st(0), compared after storing, so a caller that goes on computing with the unrounded value is not modelled until such callers are recovered. On x64 the rotations are not held to the original: see portability.

Printed, not failing: the same comparison at 24 bits gives 1.47 million differences in all, and at 64 bits some thousands, mostly `reject`. So precision does matter to this code, and the finding that the main thread is at 53 bits is what makes this source correct. A thread at another setting would not match.

## Pixel format: tests/oracle/oracle_fmt.cpp

`python -I tools/oracle_run.py --suite fmt` compares 27 functions, setters on dirty storage, over 300,000 rounds of known and random masks, and each `Read` path through a scripted tokenizer on which the original's own 0x00015880 runs. Result: no difference over 4,590,236 comparisons, 27/27 entries, 56/56 mutants. `tests/native/test_at_pixel_format.cpp` holds x64 to a hash of the original's results: it matches, 0xA24FA9E5908E6053.

Limits: `_stricmp` is compared on ASCII names; a locale's effect on it is not modelled. The C library's multibyte check at 0x00126F41 goes straight to the plain comparison when the code page word at 0x003B33D4 is 0, as it is in this harness; whether anything sets that word in the game is not read.

## Quaternion: tests/oracle/oracle_qua.cpp

`python -I tools/oracle_run.py --suite qua` compares 9 quaternion functions and `AtVector3::SetCross` and `SetDifference`, by bits at x87 precision 53, over 200,000 rounds of unit-like and edge values and 50,000 with infinities and NaNs. Rows come both from real rotations, so each branch of `SetFromRows` runs, and from any values. Result: no difference over 11,250,000 comparisons, 11/11 entries, 60/60 mutants. `tests/native/test_at_quaternion.cpp` holds x64 to a hash of the original's results for what has no sine, cosine or arc cosine: it matches, 0xA021B09021D51782. The x64 value was printed by that test before being written into it.

Limits: `AtAcos` is the host's, where the original calls its C library's at 0x00126600: their results are seen only rounded to float, and showed no difference here. On x64 `SetSlerp` and `SetAxisAngle` are checked only for what must hold. NaNs differing only in sign count as alike, as in the vector suite. Sum order is not separately tested here, for the reason given there.

## Material: tests/oracle/oracle_mat.cpp

`python -I tools/oracle_run.py --suite mat` compares 14 functions, on **all 645 distinct text cases holding a material, 2684 materials**, read twice, described, copied, reset and destroyed; on each keyword alone; each blend, compare and stencil word; indices in and out of bounds; materials read or copied over loaded ones. Result: no difference over 59,071 comparisons, 14/14 entries, 87/87 mutants. The file is tokenized once, by the recovered tokenizer, which the tok suite compares with the original's over the same files; both sides then read one scripted tokenizer, with the original's helpers such as 0x00015880 running on it. The keyword table is compared with the bytes at 0x002B15C0.

Limits: the lookup of a shader is replaced by one answer for both sides, so shader names do not reach anything real, and the material's arrays are not filled by this reader. Slot 0 of the class is a stub on both sides: what a concrete material does with its data is not recovered. The deleting destructor runs without the delete. Identical files appear once, so counts are of distinct contents.

## Material database: tests/oracle/oracle_mdb.cpp

`python -I tools/oracle_run.py --suite mdb` compares 12 functions on **all 645 distinct `.AGM` files, 2684 materials**, each loaded through the file code on both sides, loaded again over itself, copied three ways and copied with only some materials loaded, cleared and destroyed; on written text for every word and error path; on allocation from new and over what is there; on each list reader alone; on the owner's two lookups; with no heap, no memory and no file. Result: no difference over 198,832 comparisons, 12/12 entries, 110/110 mutants. One mutant was dropped as equivalent, with its reason beside the list. Lookups are real, in maps on two owners, one to a side, filled with every string in the text but names beginning with X.

Limits: slots 2, 3 and 8 are the harness's, because the concrete database is not recovered, and slot 3 is never called here. The test heap is padded by 64 KB per block, because copies can run past a list (docs/portability.md); the overrun is compared as far as the used count. Not tested: a name that is not a string where a name is required, which leaves an uninitialised stack buffer in use on both sides. Null from the heap is tested only through `Read`, with empty lists; the allocation and copy paths would then clear through null, on both sides. A file that will not open is compared up to the crash on the null tokenizer, since the harness's report returns and the game's does not. Materials on each side are read by that side's own reader; the mat suite compares those readers.

The mat suite still answers shader lookups itself, which is why `AtMaterialOwner` has a file of its own.

## Mutants in parallel

`tools/oracle_run.py` now runs mutants in `--jobs` private trees, by default up to six. Verdicts were compared with the serial run's record: identical on mat (87), tok (172) and hep (154). Safeguards: each tree passes its own positive control; a build that did not recompile the mutated file is refused; a timeout, which counts as detection, must repeat when run alone.
