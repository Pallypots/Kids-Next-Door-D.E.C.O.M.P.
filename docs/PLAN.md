# Plan (no numbers here — measured progress is in docs/PROGRESS.md)

1. **First format reader.** `Levels.txt`, then what mounts archives and what else reads text, on top of the recovered tokenizer.
2. **Rest of the first translation unit.** Sorted container at 0x000761B0-0x00076600, then what remains around it.
3. **Library separation.** Grow the string-reference map into evidence-backed ranges for LIBCMT, XAPI, CRI
   (ADX/Sofdec), libjpeg and D3DX inside `.text`; keep an `unknown` bucket; never rename from an unreviewed signature
   database. Report game/engine, middleware and SDK/CRT denominators separately.
4. **Directory listing.** `AtIoDirectoryListing` and the `List` slots, when something needs them.
5. **Application framework.** `AtAp*Application` vtables → main loop and subsystem start-up order.
6. **Close the open Ghidra disagreements** where they affect work in hand (docs/verification.md); feed high-confidence
   starts to Ghidra so that vftable-only functions become functions there too.
7. **Original toolchain (needs an authorised VC 7.1 from the owner).** Add it to config/toolchain.toml and rerun
   everything; promote functions only on its results.
