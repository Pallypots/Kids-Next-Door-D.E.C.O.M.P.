# Plan (no numbers here — measured progress is in docs/PROGRESS.md)

1. **Tool cross-check.** Ghidra + ghidra-xbe import; compare section map, entry, thunks, boundaries with
   tools/funcs.py; export a function list with Ghidra confidence for disagreements only.
2. **Library separation.** Grow the string-reference map into evidence-backed ranges for LIBCMT, XAPI, CRI
   (ADX/Sofdec), libjpeg and D3DX inside `.text`; keep an `unknown` bucket; never rename from an unreviewed signature
   database. Report game/engine, middleware and SDK/CRT denominators separately.
3. **First subsystem: Gm containers + socket/plug model.** Small, dependency-light, heavily called. Recover types
   with offsets cited to instructions; semantic tests for each method.
4. **Resource loading.** Follow the code that builds `JamFiles\XBox\%s%s.JAM` to the `FSTA` reader; recover the
   container format from code; write a host parser with bounds checks and synthetic tests.
5. **Application framework.** `AtAp*Application` vtables → main loop and subsystem start-up order.
6. **Original toolchain.** When an authorised VC 7.1 is available, add it to config/toolchain.toml and rerun
   everything; promote functions only on its results.
