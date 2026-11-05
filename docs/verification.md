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

59 tests, all passing on 2026-11-05:

* `tests/test_parsers.py` (12, synthetic): XDVDFS listing, case-insensitive lookup, explicit base, bad magic,
  truncated file, directory-tree cycle; XBE retail/debug keys, zero-fill not file-backed, entry outside code
  rejected, bad section digest reported, raw>virtual rejected, bad magic/truncation.
* `tests/test_jam.py` (18, synthetic): FSTA, FSTR and JAM2 parsing; bad token, JAM1, truncation, oversized header, child
  table outside header, directory cycle, bad name and extension index; extent, overlap, inside-header and misaligned
  record detection.
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
linked at base 0x10000 with a large executable section, so the XBE's range belongs to the image from process creation.
Original code below that section (under about 0x46000) is unavailable; the code under test starts at 0x75000.

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

Mistakes this qualification caught in the harness itself, recorded because they would have produced false confidence:
copied sources kept old timestamps, so the build reused the previous mutant's object and later verdicts were wrong
(sources are now rewritten fresh); `--mutate` triggered too rarely to ever fire; the list destructor's context clear was
invisible because the object was deleted before comparing. One candidate mutant (removing the `return` in
`GmPlug::Disconnect`) turned out to be equivalent, because `Unlink` zeroes `m_plugNext` and stops the loop anyway, and
was replaced. A surviving mutant must be explained like this, never just dropped.

## Other checks in place

* Layout `static_assert`s for the original 32-bit offsets compile under x86 modern MSVC (proved active by a
  deliberately wrong offset that fails the build).
* Host semantic tests, against recovered code with no stand-ins: `cmake -S . -B build/native -A x64`,
  `cmake --build build/native --config Debug`, `ctest --test-dir build/native -C Debug`.
