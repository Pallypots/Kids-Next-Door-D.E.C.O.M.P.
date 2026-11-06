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
