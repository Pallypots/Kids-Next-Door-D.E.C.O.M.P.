# Recovering a subsystem in a worktree

For work on one subsystem on its own branch, alongside others. CLAUDE.md governs; this is how to follow it here.

## Where things are

The owner's files are ignored by git, so a worktree has none. The tools find the main checkout's by themselves (`tools/fnconfig.py`). Treat the main checkout `C:\KND` as read only, except for scratch files in `C:\KND\analysis-local\scratch\<your subsystem>\`, which is ignored by git. Never commit anything from `orig/` or `analysis-local/`, or lists of the game's files, or disassembly.

Run from `C:\KND`, read only:

* `python -I analysis-local/scratch/dis.py 0xVA ...` disassembles, with strings.
* `python -I analysis-local/scratch/xr.py 0xVA ...` shows what refers to an address.
* `python -I analysis-local/scratch/region.py 0xLO 0xHI` lists the functions in a range.
* `python -I analysis-local/scratch/owner.py 0xVA` shows the function holding an address.

Text cases are in `analysis-local/oracle-tok/`, with `index.txt` columns: file, archive, path on the disc, size. All are distinct contents.

## What to add, and only here

| What | Where |
| --- | --- |
| Source | `src/at/` or `src/gm/`, new files; `include/at/` or `include/gm/` |
| Build | `cmake/subsystems/<name>.cmake`: `target_sources(knd_at PRIVATE ${KND_AT_SRC}/X.cpp)`, `knd_native_test(test_x)`, `knd_oracle(oracle_x)` |
| Records | `config/functions.d/<name>.toml`: `[[function]]`, `[[vtable]]`, `[symbols]`, the same fields as `config/functions.toml` |
| Oracle | `tests/oracle/oracle_<name>.cpp`, built on `tests/oracle/oracle_mdb.cpp` for a text loader |
| Mutants | `tests/oracle/suites/<name>.py`: `SUITE = {"target": "oracle_<name>", "subsystem": "at/<name>", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": [(name, file, exact text, replacement), ...]}` |
| Host test | `tests/native/test_<name>.cpp`, passing on x64 |
| Notes | `docs/worklog/2026-11-06-<name>.md` |

The `subsystem` must not be a prefix of another. Do not edit `CMakeLists.txt`, `config/functions.toml`, `tools/`, `tests/oracle/oracle_doubles.h`, `docs/STATE.md`, `docs/PROGRESS.md`, `docs/verification.md`, `docs/formats.md` or `docs/portability.md`: others are working beside you, and the integrator writes those from your notes. A small additive edit to a shared header is allowed when a class really has the member, for example a declaration in `AtMaterialOwner`; say so in your report. If you need anything else shared, write what and why in your report instead.

## Method

1. Read the loader and its neighbours. Use `include/at/AtMaterialDatabase.h`, `src/at/AtMaterialDatabase.cpp`, `tests/oracle/oracle_mdb.cpp` and `tests/oracle/suites` as the pattern.
2. Recover the functions your task names and what they alone need. Readable C++, not translated registers. Names are inferred, and say so; addresses go beside declarations. Keep the original's quirks and note them. An `AtFatal`-style report path, `AtReportFileError`, tokenizer, name maps, heap and allocator are already recovered: use them. What you do not recover stops with a diagnostic or stays a virtual slot; it does not count.
3. Differential oracle against the original, on every real file of the format, plus written text for each word and error path, each entry reached. Follow `docs/verification.md`. The original's helpers run through redirected doubles; typedef argument counts must match each `ret N`. x87: `double` stays in a register, `float` was stored. Host `/Zl` compile in `tools/verify.py`: no C library headers in `src/`; declare by hand, as the existing files do.
4. Mutants: at least one per branch and field that matters, each text occurring exactly once. Every mutant must be detected. A survivor is a gap in the harness to close, or provably equivalent: then drop it with a comment giving the reason. Never weaken an assertion or remove a failing case to get green, and never mask a difference. If a case cannot be compared, for example uninitialised stack, say so in your notes.
5. Host test on x64: `cmake -S . -B build/native-x64 -A x64`, `cmake --build build/native-x64 --config Release`, `ctest --test-dir build/native-x64 -C Release`.

## Lessons from earlier batches

* **Stay in budget.** About 60 functions to an agent. A loader's real closure can run ten times what direct calls suggest, once vftables are followed: `.ASN` looked like 40 and was 522. When you reach the budget, stop at a coherent edge, leave the rest behind a seam that stops with a diagnostic, and say plainly what a real build then does on real files.
* **Look before naming.** Search `include/` first; the owner classes especially. `AtNameTableOwner` and `AtMaterialOwner` exist. Two branches once named one class twice. Sibling methods on successive offsets are a sign of one class. If another name or class of yours may be the same as one here, say so.
* **Check your text reaches what it tests.** Written text with integers where the reader takes reals gave both sides the same zeroes, and looked green until mutants showed it. Each group of written cases should be shown to change a compared field.
* **Check a header edit against every user.** A member you change or retype can break a harness you did not touch. Before committing a shared edit, run every suite with no mutants, which also compiles them.
* **Say what a green result does not show.** If doubles stand in for the classes that matter on real files, say that first, not in a limit further down.
* **Shell pitfall.** Backslashes in `sed` and heredoc patches get eaten. Use the edit tool, or a Python file written with the write tool.
* Name worklogs by the day you start them.

## Commands, in the worktree

`python -I tools/oracle_run.py --suite <name> --no-mutants`, then `python -I tools/oracle_run.py --suite <name> --jobs 2`. Other agents share this machine, so stay at two jobs. A mutant run can pass two minutes: run it in the background and wait for its notification. Do not edit `src/` during a mutant run.

`python -I tools/verify.py --only 0xVA ...` must show MATCH, MISMATCH or SIZE for each of your functions and MATCH for each of your vftables, never MISSING or UNRESOLVED. It exits 1 regardless, since the host compiler is not the original's. A host MATCH is not a claim about the original compiler. The state of this work is Semantic-tested.

Chain gates with `&&`, never `;`. Commit on your branch, with only your own paths, only after your suite passes with every mutant detected and your host test passes. End the message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push, do not merge, and do not touch other branches.

## Report

Your branch and commit; functions and vftables recovered with addresses; exact oracle summary line, entries reached, mutants; the host test result; what is not recovered or not tested; quirks of the original worth keeping; any shared edit or request; and your own mistakes. Report failures as failures. If you could not finish, commit nothing that does not pass and say where you stopped, with what remains.
