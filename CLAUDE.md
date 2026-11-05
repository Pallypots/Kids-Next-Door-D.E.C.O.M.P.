# Project KND — True Decompilation and Native Source Port

Prepared 5 November 2026. Place this file at the root of the project as `CLAUDE.md` and ask Claude Code to execute the first-session instructions below. This is a project charter and executable work plan, not an assertion that any game code has already been recovered.

## 1. Mission and non-negotiable scope

Recover maintainable, understandable source for **Codename: Kids Next Door – Operation: V.I.D.E.O.G.A.M.E.**, using the **original Xbox release** as the primary reference. Build a true source decompilation first, with a native port growing from verified recovered subsystems. The eventual targets are Windows x86-64, Linux x86-64/Steam Deck, and Android ARM64. Preserve architectural room for macOS and other targets without promising untested support.

The owner explicitly does **not** want a static recompilation project. Do not substitute XboxXBERecomp, XenonRecomp, instruction-to-C translation, a guest-CPU interpreter, a runtime XBE loader, or an emulator wrapper as the game implementation. Do not switch the primary game to the PS2 or GameCube release without the owner's decision.

Here, “true decompilation” means recovering the original program's logic, types, data layouts, control flow, and subsystem relationships into source that can be understood, modified, and built. It cannot guarantee recovery of the original developer's exact variable names, comments, or source formatting. Compiling recovered source is necessary and is not the prohibited “recomp” approach.

Use C/C++ as the initial source-recovery hypothesis, then confirm language and ABI evidence from the executable. Automated decompiler output is a useful draft, never automatic proof of correctness. A simulator of x86 registers written in C++ is not acceptable recovered gameplay source.

Maintain two explicit build purposes:

| Purpose | Compiler/runtime | Acceptance |
| --- | --- | --- |
| Reference/matching | Proven compatible original-era toolchain, when available | Auditable comparisons against the exact chosen Xbox executable |
| Native/portable | Modern host compilers, platform backends | Verified source behavior plus platform-specific execution tests |

The native binary is not expected to match Xbox machine code. Preserve matching evidence while making portability changes traceable. A playable vertical slice does not mean the decompilation is complete.

## 2. Evidence available and unknowns

The supplied `matching-decomp.zip` contains a matching-decomp skill, Python checker, templates, and recovery/orchestration guidance. No KND executable or assets were inspected in preparing this charter. Do not invent hashes, function counts, addresses, compiler versions, middleware, file formats, or progress.

The xemu title page identifies this game as title ID `5454008a`; use this only as a corroborating clue, not an executable hash or proof of region/revision. Its compatibility reports are useful test leads, not proof of a bug-free reference. See [xemu title entry](https://xemu.app/titles/5454008a/).

Initially unknown: exact region and revision, modified versus original executable, compiler/linker version, engine identity, third-party libraries, asset containers, video/audio codecs, script representation, timing model, and whether any useful symbols remain. A studio name or another game's implementation does not establish shared engine code.

## 3. Apply the installed skill with an Xbox-specific override

Read the installed `matching-decomp` skill and relevant setup, recovery, matching, and toolchain references. Reuse its principles: verify claims, preserve state in files, isolate work, record compiler experiments, and checkpoint verified units.

**Do not run its templates blindly.** Inspection of the supplied `scripts/dk.py` found `pyelftools` object loading, a MIPS relocation table, and a header explicitly limiting relocation support to MIPS. The setup guide uses splat, ELF images, MIPS toolchains, and `INCLUDE_ASM`. These are not an established original-Xbox solution.

Before adopting its checker:

1. Inspect the actual installed version and command help; the prose uses both `check` and `try`, and examples must agree with the implementation.
2. Establish a proper x86/XBE mapping and, if the selected compiler emits them, COFF object parsing and relocation handling.
3. Either implement a tested Xbox adapter or use an independently validated checker. Do not relabel the MIPS checker as Xbox-compatible.
4. Do not apply PS2 instruction scheduling, delay-slot, register-allocation, or assembler-filter recipes to x86.
5. Keep the unmodified supplied skill as reference; put project-specific adaptations in this repository with attribution.
6. Avoid destructive rescue/reset automation. Save unfinished recovered source separately before reverting a local candidate.

This `CLAUDE.md` is the canonical project instruction file. If an `AGENTS.md` is useful for other tools, make it a short pointer rather than a second conflicting policy. Do not assume Claude automatically discovers every other agent's instruction format.

## 4. Research baseline: source decompilations and their port descendants

This is a curated cross-platform survey of relevant primary repositories, not a claim to enumerate every decompilation on the internet. Descriptions were checked on 5 November 2026. Repository claims are not independently reproduced results. Recheck the exact commit, source, build process, checker, and license before adopting anything; do not use stars or a tracker label as technical proof.

| Project and primary source | Category | Lesson for KND | Boundary |
| --- | --- | --- | --- |
| [Halo 2](https://github.com/kirklandsig/halo2-decompiled) | Original-Xbox matching C/C++ decompilation | Closest platform reference: retail-function inventory, original-compiler matching, provenance, separating game and library scope | In progress; its compiler and library assumptions do not automatically apply to KND |
| [Halo CE](https://github.com/punpckhdq/halo) | Xbox source decompilation targeting build 2342 | Study Xbox ABI, build organization, and progress reporting | Targets a particular debug build, not proof that a stripped retail KND binary has comparable metadata; audit provenance before reuse |
| [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) | Port derived from the Halo decompilation lineage | Repository describes Windows/Linux/Android targets; examine platform separation and inherited limitations | Port descendant, not an independent recovery of every function; do not equate published platform claims with our own validation |
| [Super Mario 64](https://github.com/n64decomp/sm64) | Full source decompilation for named original releases | Explicit revision selection, reference hashes, asset extraction, recovered source organization | N64/MIPS toolchain does not transfer directly |
| [sm64-port](https://github.com/sm64-port/sm64-port) | Decompilation-derived native port | Concrete example of recovered source targeting native Windows/Linux | Portability work is distinct from original-console matching |
| [Ocarina of Time](https://github.com/zeldaret/oot) | Source decompilation | Versioned builds and requirement for user-supplied assets | Original-game reconstruction is distinct from an enhanced PC distribution |
| [Shipwright / Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) | Decompilation-derived enhanced port | User asset import, native packaging, settings, input, and enhancement separation | Reference for the later port; not an Xbox recovery tool |
| [The Wind Waker](https://github.com/zeldaret/tww) | GameCube C++ decompilation | Object-level diff workflow; its documentation explicitly cautions against directly trusting Ghidra output | PowerPC and compiler specifics differ |
| [Pokémon Emerald](https://github.com/pret/pokeemerald) | Source decompilation building a specified ROM | Deterministic reference target, organized data/source, diff tooling | ARM/GBA platform and data policies are not KND defaults |
| [REDRIVER2](https://github.com/OpenDriver2/REDRIVER2) | Driver 2 reverse engineering/source-port effort | Recovered systems can support a maintainable enhanced port | Do not infer exact matching from playability or assume its symbol advantages exist here |
| [OpenGOAL](https://github.com/open-goal/jak-project) | Language-aware decompilation and native runtime/compiler project | Recover types, validate generated source, separate tools/assets/runtime | GOAL-specific tooling is not a general C++ Xbox decompiler; do not copy its architecture wholesale |

The common project recommendation inferred from these references: identify one exact original build; recover and verify source; document dependencies; port behind explicit platform boundaries; add enhancements only after fidelity is measurable.

Explicit exclusions: XboxXBERecomp and its BLACK port are static-recompilation examples, not the foundation requested here. [XboxXBERecomp's own description](https://github.com/TGP482/XboxXBERecomp) explains its ahead-of-time x86-to-C++ approach. Similarly, do not treat an “open” game name, a port announcement, or a progress/planning repository as evidence of a completed decompilation. The ReBLACK-XBOX repository could not be directly inspected in this research, so it is not relied upon as a verified implementation reference.

## 5. Initial tools: qualify before relying on them

| Tool/source | Intended use | Required qualification |
| --- | --- | --- |
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) and [XboxDev ghidra-xbe](https://github.com/XboxDev/ghidra-xbe) | XBE import, disassembly, cross-references, type recovery, scripted exports | Test compatible versions, mapped addresses, decoded entry point and thunks; inspect plugin/signature provenance |
| [XboxDev XBE documentation](https://xboxdevwiki.net/Xbe) | Format and loader-field reference | Verify against our actual input; distinguish file offsets from virtual addresses |
| [extract-xiso](https://github.com/XboxDev/extract-xiso) | Candidate disc extraction tool | Detect the actual container first; preserve input and verify extracted files |
| [objdiff](https://github.com/encounter/objdiff) | Candidate visual object comparison aid | Confirm x86/object-format support for the chosen version; not automatically the authoritative checker |
| [nxdk](https://github.com/XboxDev/nxdk) | Open original-Xbox SDK reference and possible test-harness tool | Not a replacement guaranteed to reproduce proprietary compiler output or the game's original library ABI |

Use a small reproducible toolchain. Prefer Python for extraction/inventory/checking and CMake plus Ninja for modern host builds if the environment supports them. A legacy reference build may need a separate driver. Record versions and commands rather than assuming current package names or downloading arbitrary binaries. Use authorized tool installations; never fetch leaked game source, symbols, or SDK packages.

## 6. First session: execute this sequence

Begin work rather than returning only a proposal. Inspect existing repository instructions and uncommitted changes; preserve unrelated work. Identify host OS, CPU architecture, installed compilers, Python, build tools, and available analysis tools.

### A. Establish the reference input

- Search only the designated project/input directories for the owner's files. Accept an extracted game directory, XISO, or full disc image as inputs to identify; do not reject by extension alone or demand another dump unnecessarily.
- Preserve the input unchanged. Record file sizes and SHA-256 hashes, extraction tool/version, and provenance in a local manifest. A hash is an identity check, not proof of correctness unless compared to an independently trusted reference.
- Locate `default.xbe` and other executables/modules. Identify the title, revision/region evidence, header/section mappings, and signs of patches. Never silently treat a patched XBE as retail.
- Select one reference build and give it an unambiguous internal ID. Keep other releases separate. Never mix addresses or assets between revisions.
- If the game is missing, ask for the path to the owner's copy in plain language. Meanwhile implement the scaffold, synthetic parser tests, and input validation. Do not manufacture binary findings.

### B. Implement the foundation

- Create `.gitignore` before extraction or generation. Keep discs, executable copies, assets, SDK contents, disassembly, Ghidra projects, and private traces out of commits.
- Implement read-only executable inventory with range/bounds checks and explicit failures on malformed input. Inventory raw sections and mapped/zero-filled ranges separately. Decode entry-point/kernel-thunk fields according to the detected XBE variant.
- Import into Ghidra with the verified loader. Cross-check representative raw bytes, section addresses, entry point, imports, and strings independently.
- Generate initial function/data inventories, with confidence labels. Variable-length x86 instructions, jump tables, alignment, tail calls, thunks and shared tails make naive “every label is a function” counts unreliable.
- Identify likely CRT/XDK/middleware separately from game/engine code using recorded evidence. Signature matches need corroboration; no blanket renaming from an unreviewed database.
- Look for RTTI/vtables, constructors/destructors, exception handling, TLS, static initialization, strings, script tables, and source paths. Label inferred names as inferred.

### C. Prove the verification loop

- Investigate compiler families/builds, optimization flags, calling conventions, structure packing, C++ name mangling, x87/SSE behavior, and linker layout. Treat MSVC-era compilation as a hypothesis until supported.
- Select 3–5 small real functions spanning more than trivial returns: arithmetic/branching, structure access, a call/relocation, and floating point if possible. Recover readable source and compare actual compiler output.
- Qualify the checker with positive and negative controls before trusting its verdicts. Wrong constants, reversed conditions, wrong call targets, altered data references and removed functions must fail or report missing.
- If the original-compatible compiler is unavailable, keep the matching milestone explicitly blocked. Continue analysis, portable semantic recovery, and behavior tests without calling them byte matches or converting to recompilation.
- Deliver at least one real recovered/verified function when inputs permit; otherwise deliver working input tooling and an exact evidence-based blocker. Do not make a copied binary or empty shell the first “decompilation success.”

## 7. Verification and honest progress

Each function record must include reference-build ID, address/range, size, owning subsystem, source path, confidence, compiler/flags, verification command, result, and evidence paths. Record all split/shared fragments and references needed to interpret a comparison.

Use these distinct states:

| State | Meaning |
| --- | --- |
| Unanalyzed | No validated recovery |
| Analyzed | Boundaries/behavior investigated |
| Draft source | Recovered candidate; not validated |
| Semantic-tested | Passes specified behavior tests; not proof of all possible behaviors |
| Relocation-aware match | Instructions/data match under documented relocation/symbol equivalence rules |
| Exact linked match | Rebuilt bytes match at the original addresses for the stated range |
| Native-integrated | Recovered source runs in the port and passes integration checks |

Match data references and target identities, not just masked opcode bytes. A relocation-aware comparison must reject wrong symbols/addends; do not mask every address or immediate. Preserve jump-table targets, vtables, static constants, initialization and ABI metadata in the verification scope. Object-level matches alone do not establish a whole-program match.

Whole-XBE reconstruction needs its own milestone. Preserve or explicitly account for headers, padding, section checksums, signing-related fields, and layout differences. Any normalized comparison must publish precise excluded fields and the reason. Call it normalized, not byte-identical. A mixed source/assembly reconstruction validates the pipeline; the retained assembly is still undecompiled. Copying original bytes back into a candidate never counts as recovery.

Report function counts and unique code-byte coverage separately. Keep game/engine, middleware, and SDK/CRT denominators visible, plus unknown/unclassified ranges. Do not double-count aliases/shared tails, drop hard functions from scope, count stubs, or inflate progress with library wrappers. Version denominator changes with a reason. Native release readiness is a separate test matrix, not a percentage inferred from matching.

Never alter expected hashes, weaken assertions, exclude failures, or patch compiler output with original instruction bytes to obtain green status. Checker changes require focused regression tests, negative controls, and rerunning all affected matches. Keep benchmark input fingerprints and verifier version in every report.

## 8. Recover the game by subsystem

Choose work from the real call graph, not a fixed list of guessed function names. Prioritize small dependency-light functions, file/archive handling, math/data structures, object lifecycle, resource loading, and the main update loop. Then recover animation, collision/physics, cameras, character control, AI, scripted events, UI, audio, cinematics and saves as evidence supports them.

Use original execution as a behavior oracle where feasible: hardware or a separately identified emulator build, deterministic input sequences, test saves, traces, and screenshots. Emulator use for analysis is allowed; emulator dependency in the delivered game implementation is not. Emulator deviations must not become the game's specification.

For each subsystem: inspect instructions/callers/data; propose types with confidence; implement readable source; compile; compare; exercise boundary cases; document unresolved differences. Never guess a mission script, return a constant from an unimplemented API, or suppress a crash and label the system recovered.

Unknown native functionality should fail clearly with a diagnostic. Keep stubs out of completion counts and release gates. Bounded investigations should end with evidence and the next experiment, not endless random source permutations.

## 9. Native architecture and Android readiness

Recover faithfully while avoiding new platform dependence. Organize by evidence-backed subsystems; do not prebuild a giant replacement engine. Keep original layouts and port adapters separate and retain a single shared logic implementation where practical. Document unavoidable source variants explicitly.

| Boundary | Design requirement |
| --- | --- |
| Game logic | No Win32, Xbox kernel, GPU or Android APIs embedded in ordinary gameplay code |
| Binary data | Fixed-width fields, explicit parsing, bounds checks, byte-order handling; never deserialize 32-bit Xbox pointers as host pointers |
| Platform services | Small interfaces for files, clocks, threads, synchronization, input, audio and save paths |
| Graphics | Recover the game's render semantics; isolate a modern backend after investigating actual shaders/state/texture formats |
| Assets | User-supplied originals imported reproducibly; versioned cache; no copyrighted assets bundled |
| Native build | Windows/Linux x86-64 first; ARM64 compilation tests early to expose pointer, alignment and intrinsic assumptions |
| Android | Native ARM64 code, lifecycle/background-resume handling, supported file-picker/storage flow, audio focus, controller reconnect and GPU capability testing |

Evaluate SDL and a suitable cross-platform rendering layer against observed needs; record the decision before making one API mandatory. Do not bake a Windows-only graphics/audio dependency into the shared core. Android graphics support and minimum OS/device requirements must be measured and documented, not guessed.

Do not recreate a universal Xbox emulator to get the game running. A bounded compatibility adapter for known platform services is acceptable; the final game logic must execute recovered source. No final dependency on executing original x86 game instructions or an x86 translation layer on ARM64.

Test x87 precision differences, signed overflow assumptions, aliasing, struct padding, endianness, SIMD and unaligned accesses. Use sanitizers where suitable and explicit save serialization. Do not cast host pointers into 32-bit integers. Avoid fixed-address and page-size assumptions unless proven necessary and isolated.

## 10. “Ultimate port” backlog, after fidelity

- Preserve original gameplay speed and offer a fidelity baseline. Recover timing first; only then add higher render rates/interpolation. Never speed up physics, scripts or animation by changing one FPS constant.
- Widescreen/ultrawide and resolution scaling with correct projection, UI anchoring, menus, subtitles, cutscenes, loading screens, culling and camera behavior.
- Remappable gamepad and keyboard/mouse input, controller glyph selection, deadzones, vibration and handheld-friendly menus.
- Android touch controls that can be fully hidden, independently hideable overlay/menu buttons, and an accessible way to bring them back. A connected handheld controller should not leave touch controls obstructing the screen.
- Versioned saves, backup before migration, robust pause/resume, settings that persist, diagnostics and reproducible crash reports.
- Optional texture replacement/mod interfaces, camera/photo features and accessibility improvements once the relevant formats and systems are understood. Preserve original assets and behavior as selectable defaults.

These are goals, not promised launch features. Implement enhancements behind settings and tests; avoid destabilizing the recovery branch.

## 11. Milestones with exit evidence

| Milestone | Required result |
| --- | --- |
| M0: Identity | Immutable input fingerprint, extraction record, verified XBE map, environment report |
| M1: Trustworthy tools | Real inventory, tested checker, negative controls, compiler investigation and initial recovered functions or a precise toolchain blocker |
| M2: Reference reconstruction | Reproducible assembly/source build where feasible; comparison scope and exceptions explicit; no false decompilation credit |
| M3: Verified subsystem | Meaningful recovered logic with type documentation, matching/semantic evidence and host tests |
| M4: Native vertical slice | Real imported level content, character input, camera, collisions, animation and representative event/audio behavior; failures documented |
| M5: Whole-game fidelity | Full progression and ending tested; every level, playable character, ability, boss, checkpoint, save/load path and cinematic inventoried and exercised |
| M6: Platform qualification | Windows/Linux runs plus actual ARM64 Android device tests, installation, input, saves, suspend/resume and performance evidence |
| M7: Enhanced release | Optional improvements tested, asset-free packaging, reproducible builds, supported-input list, notices and known issues |

M2 toolchain difficulties must not stop independent source recovery or host testing. Conversely, a successful M4 does not waive unresolved source/matching work. State whether each gate passed, is partial, blocked, or untested. Do not promise a completion date before measuring scope and recovery throughput.

## 12. Repository, continuity, and publication

Suggested starting directories, creating them only as needed:

- `src/` and `include/`: recovered code and documented types.
- `platform/`: native backends and explicit compatibility adapters.
- `tools/`: input inspection, extraction, analysis exports, comparison and progress tooling.
- `config/`: reference-build metadata and build configuration, without proprietary payloads.
- `tests/`: synthetic fixtures, verifier negative controls, recovered-function and integration tests.
- `docs/`: `STATE.md`, `PLAN.md`, `toolchain.md`, `verification.md`, `provenance.md`, `formats.md`, `portability.md`, and worklogs as needed.
- Ignored `private/`, `orig/`, `build/`, `analysis-local/`: owner inputs and generated/private material.

At session start read this file, `docs/STATE.md`, active blockers, latest verification output, and working-tree status. At session end record what actually changed, commands and results, unresolved hypotheses, input/tool fingerprints, and the next concrete task. Generate metrics from the inventory. Mark missing measurements unknown.

Commit focused verified units locally when a repository is available, staging only owned paths. Do not erase unrelated work, rewrite history, upload private inputs, publish releases, or push without existing authorization. Before a public commit/release inspect staged contents for original executables, assets, proprietary SDK files, generated disassembly and sensitive local traces. Preserve licensing and attribution for reused open-source code; permission to study code does not automatically permit copying it under our chosen license.

Describe the work as independent reverse engineering with documented provenance. Do not claim a formal clean-room process unless one actually exists. Do not use leaked source, leaked symbol databases, or confidential material. Public availability alone is not provenance. Keep private game bytes out of public CI; use synthetic checks publicly and authorized local/private validation for original-binary comparisons.

## 13. Immediate instruction to Claude

Execute Section 6 now, adapting to the actual workspace. Start with a short plain-language update, then inspect and implement. Use the installed matching-decomp skill subject to this file's Xbox-specific corrections. Keep working on unblocked tasks and ask only for genuinely missing inputs or material project decisions.

At the end of the first session, report:

1. Which files and reference build were actually found, or exactly what input is missing.
2. Which tools/checks work, including negative-test results.
3. What source was recovered and its exact verification status.
4. The most important confirmed technical findings and remaining uncertainties.
5. The next concrete engineering task.

Do not report “decompilation started” merely because folders exist. Deliver evidence, working tools, and recoverable project state. The long-term goal remains a true source decompilation and a high-quality native Windows/Linux/Android port.
