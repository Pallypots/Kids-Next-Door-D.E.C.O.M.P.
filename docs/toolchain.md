# Toolchain investigation

## Evidence about the original compiler (reference build knd-xbox-usa-5454008a-v1)

| Observation | Where | Supports |
| --- | --- | --- |
| 118 MSVC RTTI type descriptors (`.?AV…@@`), complete object locators and class hierarchy descriptors in the MSVC layout | `.data` / `.rdata`, tools/funcs.py `rtti()` | MSVC C++ compiler family, RTTI enabled (`/GR`) |
| MSVC C++ name mangling in type names (`?$GmLinkedListOf@PAVFaUiBase@@`) | same | MSVC |
| `push -1; push <handler>; mov eax, fs:[0]` prologues (C++ EH frames), `__thiscall` (`this` in ECX), callee-cleans `ret N` | `.text` (320+ / 350+ starts) | MSVC x86 code generation, C++ exceptions enabled |
| Functions 16-byte aligned, padded with `0xCC` (2,273 of the call-target starts are preceded by `0xCC`) | `.text` | MSVC linker/compiler alignment (exact flag not yet identified) |
| LIBCMT 1.0.5849 library record; CRT message about the ".NET Runtime" | XBE header; strings | CRT from the Visual C++ .NET generation, shipped with the XDK |
| Core vftables (`GmSocket`, `GmPlug`, `GmLinkedList`, ... at 0x00278418-0x0027857C) have no RTTI locator before them, while `GmLinkedListIterator` (0x0027BCC4) and `Fa*` vftables do | `.rdata` | Mixed RTTI settings: at least some engine translation units built with `/GR-`, game ones with `/GR` (a COMDAT vftable keeps its locator if any contributing object had one). Inferred; not proven per unit |
| Direct call from `~GmLinkedList` to 0x000DF720, a plain `[ecx+4]` setter that also sits in unrelated `Fa*` vftables; `0x000CBF30` (`ret 4`) and `0x00092C10` (`xor eax,eax; ret 8`) shared across many classes | `.text`, vftables | Linker identical-code folding (`/OPT:ICF`). Function identity by address is therefore not unique |
| Alignment padding inside functions as `lea ebx,[ebx]`, `lea ecx,[ecx]`, `lea esp,[esp]`, `mov edi,edi` | e.g. 0x00076F9A | An older code generator; 19.44 emits other padding |
| Register-cached zero stores in some functions (`xor ecx,ecx; mov [eax+4],ecx`, 0x0007CE30) but immediates in others (`mov dword [ecx+4],0`, 0x0007CE78) | `.text` | Unexplained; also seen as a size difference against 19.44 |
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

Flags for `msvc14-host`: `/O2 /Oy /GS- /Gy /Zl /arch:IA32 /GR /EHsc /Zc:threadSafeInit- /Zc:sizedDealloc-
/Zc:implicitNoexcept- /std:c++14`, no include path. `/arch:IA32` keeps x87 floating point (19.44 defaults to SSE2).
`/Gy` puts each function in its own COMDAT section for extraction. The two added `/Zc` flags restore pre-C++11
destructor behaviour: a plain `operator delete` call and no implicit `noexcept`.

Flag scan (`python -I tools/flagscan.py`, 2026-11-05; matches out of 47 recorded functions):

| Flags | Matches |
| --- | ---: |
| baseline `/O2` | 14 |
| `/O2 /Ob1` | 15 |
| `/Ox` | 14 |
| `/O2 /Oi-` | 14 |
| `/O1` | 10 |
| `/O2 /Os` | 10 |
| `/Oy-` | 7 |
| `/Gy-` | 3 (the checker's extraction relies on per-function sections) |

`/Ob1` wins by one and was rejected: the original inlines non-`inline` destructors into their deleting wrappers
(0x00076CE0), which needs automatic inlining, yet does not inline `Allocate` into `Append` (0x00077060). No flag
reproduces both, which points at the compiler rather than a flag. The scan stops here: tuning a modern compiler further
would fit the wrong compiler.

Source organisation does matter, and it is evidence rather than tuning: functions were moved into files following the
original address clusters, because 19.44 would otherwise inline across what were separate translation units (`Contains`
into `IsConnectedTo`).

## Blocker and next experiment

To claim matches with the original toolchain we need an authorised copy of an original-era MSVC (e.g. Visual Studio
.NET 2003 owned by the project owner). We will not download SDKs/compilers from unofficial sources. Once available:
add a `[compiler.vc71]` entry and rerun `tools/verify.py --compiler vc71`. The functions that differ under 19.44 are
the first test set: if VC 7.1 reproduces them, the hypothesis is confirmed.

While this is blocked, `tools/oracle_run.py` checks behaviour against the original code (docs/verification.md), so
recovery does not stop. It does not replace matching.
