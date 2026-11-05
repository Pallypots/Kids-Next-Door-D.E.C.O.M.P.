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

Scope limits (known, deliberate): one function at a time; does not verify vtables, jump tables in data, static
initialisers or linker layout — those need their own checks before any whole-XBE claim.

## Qualification — `python -I -m unittest discover -s tests -v`

29 tests, all passing on 2026-11-05:

* `tests/test_parsers.py` (12, synthetic): XDVDFS listing, case-insensitive lookup, explicit base, bad magic,
  truncated file, directory-tree cycle; XBE retail/debug keys, zero-fill not file-backed, entry outside code
  rejected, bad section digest reported, raw>virtual rejected, bad magic/truncation.
* `tests/test_xcheck.py::CompareCore` (10, synthetic): exact match; wrong constant; reversed condition; REL32
  correct; REL32 wrong target; DIR32 data reference right/wrong; DIR32 addend respected; unknown symbol →
  UNRESOLVED; unsupported reloc; truncated and padded candidates → SIZE.
* `tests/test_xcheck.py::RealObjects` (7, local only — needs the owner's XBE and MSVC): real compiled variants:
  positive control `Next` → MATCH; `->prev` instead of `->next` → MISMATCH; reversed condition → not MATCH;
  correct call → MATCH and the same call with the symbol mapped to the sibling function → MISMATCH; call with an
  empty symbol map → UNRESOLVED; function absent from the object → MISSING; correct source checked at the
  neighbouring address → MISMATCH.

Any checker change must keep all of these passing and add a regression test for the change. Rerun
`tools/verify.py` after any checker or toolchain change.

## Running

```
python -I tools/verify.py            # compile config'd sources, check every function, write build/verify-report.json
python -I tools/xdiff.py <obj> <symbol> <va> --size N   # side-by-side diagnostics (no verdict)
python -I tools/report.py            # regenerate docs/PROGRESS.md from the measured data
```

The report embeds the XBE SHA-256, the checker SHA-256 and the compiler version line.

## Other checks in place

* Layout `static_assert`s for the original 32-bit offsets compile under x86 modern MSVC (proved active by a
  deliberately wrong offset that fails the build).
* Host semantic tests: `cmake -S . -B build/native-x64 -G "Visual Studio 17 2022" -A x64`,
  `cmake --build build/native-x64 --config Release`, `ctest --test-dir build/native-x64 -C Release`.
