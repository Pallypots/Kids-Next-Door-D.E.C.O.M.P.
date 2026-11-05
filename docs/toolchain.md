# Toolchain investigation

## Evidence about the original compiler (reference build knd-xbox-usa-5454008a-v1)

| Observation | Where | Supports |
| --- | --- | --- |
| 118 MSVC RTTI type descriptors (`.?AV…@@`), complete object locators and class hierarchy descriptors in the MSVC layout | `.data` / `.rdata`, tools/funcs.py `rtti()` | MSVC C++ compiler family, RTTI enabled (`/GR`) |
| MSVC C++ name mangling in type names (`?$GmLinkedListOf@PAVFaUiBase@@`) | same | MSVC |
| `push -1; push <handler>; mov eax, fs:[0]` prologues (C++ EH frames), `__thiscall` (`this` in ECX), callee-cleans `ret N` | `.text` (320+ / 350+ starts) | MSVC x86 code generation, C++ exceptions enabled |
| Functions 16-byte aligned, padded with `0xCC` (2,273 of the call-target starts are preceded by `0xCC`) | `.text` | MSVC linker/compiler alignment (exact flag not yet identified) |
| LIBCMT 1.0.5849 library record; CRT message about the ".NET Runtime" | XBE header; strings | CRT from the Visual C++ .NET generation, shipped with the XDK |
| Small leaf functions match MSVC 19.44 byte-for-byte, others differ only in register choice / scheduling | tools/verify.py results | the ORIGINAL compiler is an older MSVC; codegen differs from 19.44 in systematic ways |

**Hypothesis (not established):** the game code was compiled by Visual C++ .NET 2003 (7.1, `cl` 13.10.x) as used
with XDK 5849, at `/O2`-class optimisation with frame-pointer omission. Treat as a hypothesis until an
original-era compiler reproduces a non-trivial function that 19.44 does not.

Observed 19.44-vs-original differences (6 functions, `python -I tools/xdiff.py …`):

* temporaries: the original uses EDX where 19.44 reuses EAX (`First`, `Last`);
* null-test idiom: original `mov edx,[ecx+8]; xor eax,eax; test edx,edx; sete al`, 19.44 `xor eax,eax; cmp [ecx+8],eax; sete al` (`IsEmpty`);
* 19.44 copies `this` into EDX (`mov edx,ecx`) where the original addresses through ECX directly (`NextData`, `PrevData`);
* store scheduling inside a loop and the register for the final load (`At`).

These are compiler differences, not source differences, as far as can be told. Source is **not** permuted to fit
19.44; that would fit the wrong compiler.

## Available compilers

| ID | Compiler | Status |
| --- | --- | --- |
| `msvc14-host` | MSVC 19.44.35229 x86 (`config/toolchain.toml`) | installed; used for the relocation-aware loop; **not** original-era |
| original-era (VC 7.1 / XDK) | — | **missing — matching milestone blocked** |

Flags for `msvc14-host`: `/O2 /Oy /GS- /Gy /Zl /arch:IA32 /GR /EHsc /Zc:threadSafeInit- /std:c++14`, no include path.
`/arch:IA32` keeps x87 floating point (19.44 defaults to SSE2). `/Gy` puts each function in its own COMDAT section
for extraction.

## Blocker and next experiment

To claim matches with the original toolchain we need an authorised copy of an original-era MSVC (e.g. Visual Studio
.NET 2003 owned by the project owner). We will not download SDKs/compilers from unofficial sources. Once available:
add a `[compiler.vc71]` entry and rerun `tools/verify.py --compiler vc71`. The 6 functions that differ under
19.44 are the first test set: if VC 7.1 reproduces them, the hypothesis is confirmed.
