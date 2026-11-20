# The graphics factory's start (subsystem `at/grfactory`), 2026-11-19

For the integrator: this holds what belongs in docs/startup.md, docs/STATE.md and docs/verification.md. Reference build knd-xbox-usa-5454008a-v1. State of
everything here: Semantic-tested. A host MATCH from `tools/verify.py` says nothing about the original compiler (blocked, docs/toolchain.md).

Built on at/grlayer (worktree-agent-ac8c45fe27a0e03a5, 530d279) with at/gfxdev merged (worktree-agent-aa420aebf46093563, ccd0bb5). `at/grfactory` is
not a prefix of another subsystem, nor another of it.

**What a green result does not show, first.** Direct3D is a double on both sides of the oracle: the six SDK entry points the start reaches answer from a
script the harness writes. So the oracle shows what the factory does with what Direct3D reports, never what Direct3D reports on an Xbox. The device
slot 3 makes is compared only in its call log and its creator word (the device itself is gfxdev's suite's), and its texture members are the original's
constructors on both sides. On the host, `platform/host_d3d.cpp` answers Direct3D with the truth about a host (one adapter, no Xbox device
capabilities), so the real start-up keeps an adapter with no modes and no caps: nothing past this point has seen a display mode.

## The merge

`git merge --no-ff worktree-agent-aa420aebf46093563` was textually clean, but the two branches had each recovered `AtDisplayMode` (constructor
0x00043480): grlayer in include/at/AtGraphicsLayer.h (fields m_unknown00, m_unknown04) and gfxdev in include/at/AtDisplayDevice.h (m_width, m_height,
and Clear 0x000434A0). Both were recorded, so the build would have had two definitions and two records. Resolved by keeping gfxdev's: grlayer's struct,
constructor and record (config/functions.d/grlayer.toml) are gone, AtGraphicsLayer.h includes AtDisplayDevice.h, and grlayer's two mutants of that
constructor are dropped (gfxdev's suite has the same two). grlayer's mutants were rerun after (below).

Also found: gfxdev's notes call the device's maker "0x000EBE90 (slot 25 of the class at vftable 0x00288B88)". It is slot 3 of the factory's vftable
0x00288BE0; 0x00288B88 + 25 * 4 is that same word, counted from the wrong start.

## Where a real build stops now

`test_app C:\KND\analysis-local\levels` (and with no directory), x64:

    main stopped: Atlas Graphics: the texture at vftable 0x00289768 (0x000FCEE0) is the texture work's, not recovered here
    memory reported: 4294967295 total, 0 overhead; I/O context: 6 file systems, 8 buffer blocks of 32768 bytes

| After | Stop |
| --- | --- |
| (start: grlayer + gfxdev) | the graphics factory's start (0x000C7C60) |
| the factory's start, without a Direct3D answer (test_atap) | `Graphics factory: Direct3DCreate8 (0x00206070, SDK) is not in this build` |
| with platform/host_d3d.cpp (test_app) | the device's first texture member, in AtGraphicsDeviceXbox's constructor |

So the graphics layer's Init now runs the real start: the saved globals restored, both function tables filled, the factory's adapter list made (one
adapter, its identifier, names and first device info), the globals saved; then the factory's slot 0 (the adapter list, App+0x5F94) and slot 3, which
allocates 0x305C0 bytes from 0x003B13CC's heap and runs the recovered `AtGraphicsDeviceXbox` constructor. That constructor stops at its first texture
member (gfxdev's seam): the texture work's three classes are what stands between start-up and the controller layer.

**No disc file is read yet**, App.ini included: App.ini is read later in App::Init, after the whole AtAp chain. Nothing in the factory opens a file.

**Not started: the controller, sound, video and scene layers and App slot 56**, which the brief also named. The factory's closure took the budget (44
records and 4 mapped symbols), and with the device stopping in its texture members none of those layers can run in a real start-up until the texture
work is merged. Closures measured for whoever takes them (direct calls, the fatal path done; `analysis-local/scratch/grfactory/closure.py`):
controller 20 functions (1,139 bytes; its start 0x000C7B80 only calls the empty 0x001CC9F0 on a static at 0x003ADF5C), sound 21 (1,110; its start
0x000C7760 is 0x000EA2C0 and 0x000E9BB0 on the static 0x003ADF44, likely DirectSound), video 24 (542), scene 27 (2,605; 0x000C7400, 0x000C7190 and the
0x0003FF80-0x00040770 cluster), App slot 56 (0x0008DB30) 1 plus what its config's slot 0x3C reaches.

## Recovered (44 functions), include/at/AtGraphicsFactory.h

* The start, src/at/AtGraphicsFactoryStart.cpp: AtGraphicsStartFactory 0x000C7C60, AtGraphicsStopFactory 0x000C7C90, AtGraphicsFillTables 0x000EC670,
  AtMatrixTableFill 0x000EC2B0; the static factory g_atGraphicsFactory (0x003ADF64).
* The factory (vftable 0x00288BE0, 0x14 bytes), moved from AtGraphicsLayer.h: constructor 0x000EBD70, destructor 0x000EBD90, Start 0x000EB520, Stop
  0x000EB530; in src/at/AtGraphicsFactorySlots.cpp CreateDevice (slot 3) 0x000EBE90. Slot 0 (0x000EB540, `lea eax, [ecx + 4]`) and slot 4 (0x000EB9D0)
  are recovered but folded with functions recorded elsewhere (GmRumbleEffect::Name, AtIoDeleteObject): symbols only. Likewise the device info's slots 1
  and 3 (0x000FBA70, AtGroupBoundingGeometry::Overlaps).
* The adapter list (vftable 0x00289428 on base 0x00289414, 0x10 bytes): constructor 0x000FA5A0, destructor 0x000FA770, deleting 0x000FA910, Clear
  0x000FA5C0, Init 0x000FA7F0, At 0x000FA580; the base's destructor 0x0011C840 and deleting destructor 0x000FA500. Count (0x00154540) is folded, recorded
  in at/matdev.
* The adapter (vftable 0x0028943C on base 0x0028B194, 0x84C bytes): constructor 0x000FA9A0, destructor 0x000FACB0, vector deleting 0x000FA520, Init
  0x000FAA30, Clear (slot 0) 0x000FABB0, slots 2-4 0x000FAC20/30/40; the base's constructor 0x0011C850, destructor 0x0011C870, slot 0 0x0011C880,
  deleting 0x0011C8C0.
* The device info (vftable 0x00289698 on base 0x0028B1B8, 0x14C bytes; three in each adapter): constructor 0x000FBD40, destructor 0x000FC290, deleting
  0x000FC530, Clear 0x000FBD80, Enumerate 0x000FC320, ReadCaps 0x000FBF60, slots 2 and 4 0x000FBA80/0x000FBA90 (slots 1 and 3, 0x000FBA70, are folded:
  a symbol); AtGfxCapsToFlags 0x000FBBE0; the base's constructor 0x0011C950, destructor 0x0011C8E0, Reset 0x0011C900, deleting 0x0011C9C0.
* AtD3DFormatToPixelFormat 0x001197D0 with its two tables (0x0028A930, 0x0028AC40) as data; AtDisplayMode::Set 0x00043420.

No vftable claimed: the factory's has 31 slots that stop, the adapter list's two (Best 0x000FA600, Find 0x000FA700), the others' pure base slots are
the original's purecall. The six vftables are mapped in `[symbols]`. `tools/verify.py --only` on the 44: 12 MATCH, 2 MISMATCH (AtGraphicsFillTables,
whose host table has another layout; AtGfxDeviceInfo::Clear, register choice), 30 SIZE; none MISSING or UNRESOLVED.

Names, all inferred. Direct3D's roles from what the SDK functions do: 0x00206070 returns 1 for 0, as the Xbox's Direct3DCreate8 hands back a dummy
interface; 0x00206080 fails for an adapter but 0 and copies 0x42C bytes (D3DADAPTER_IDENTIFIER8); 0x002062B0 fills 0xD4 bytes (D3DCAPS8), whose fields
ReadCaps reads at D3DCAPS8's offsets; the modes are D3DDISPLAYMODE records. The first format table's flag is AtPixelFormat::kSwizz, so it is the
swizzled formats, the second the linear (D3DFMT_LIN_*) and compressed ones. "Adapter", "device info" are roles, not the original's words.

Seams, not counted: src/at/AtGraphicsFactoryD3d.cpp (the six Direct3D entry points: stop unless answered), src/at/AtGraphicsFactorySeams.cpp (six of the
render table's seven functions, 0x00117130 ... 0x001175A0, whose addresses only are written; the third is the empty 0x001CC9F0, folded, mapped; and the
adapter list's slots 3 and 4). Port code, not recovered: platform/host_d3d.cpp, linked into test_app.

The start's code is in three files (start, factory, slots 0/3/4) so that a harness of the layer above can supply the start and the slots it stands in
for (tests/native/test_grlayer.cpp constructs a factory and defines slots 0, 3, 4 and the start itself).

## Oracle (`tests/oracle/oracle_grfactory.cpp`, suite `grfactory`)

`NO DIFFERENCE OBSERVED: 6 crashes on both sides; 172892 comparisons, 0 differences`; entries reached in every run: 44 / 44 (armed by the runner's
entries file); `--mutate` fails as it must. **Mutants: 170 / 170 detected.** The first run had 173 and left 11 survivors: 3 provably equivalent, now
dropped with their reasons in the suite (Restore after the fill; the factory destructor's Clear, repeated by the list's destructor; CopyWide's
terminator, rewritten by AtTextUnicode::Init), and 8 harness gaps, closed: the memory globals moved after the save so the start's Restore shows; the
shader versions differing in their low halves; a second enumeration whose caps call fails after one of another type; the device infos' texts given
contents before Reset and the destructors; the adapter's third device info enumerated before Clear; and allocation failing for the names and the modes.

The six crashes are three deliberate cases, each the same on both sides: allocation failing in slot 3, in the adapter's names, and in the modes. The
original raises -4 (fatal in the game; a double here that returns) and then writes through the null pointer; the recovered code does the same, and the
logs up to the crash are compared.

Cases. Formats: every code of both tables, twenty more and twenty random, each with swizzled 0, 1 and 5, into a pixel format of garbage. The mode's Set
on garbage. Caps to flags: 60 random caps onto flags 0 or random. Device infos (48 passes): constructed on zeroes or 0xA5, flag words filled, then
Enumerate once to three times (a second clears the first) over scripts: identifier names of 0 to 510 characters with high bytes, the caps call failing
or not, random caps with the extents adjust 0, -0, 1.5 or a NaN, shader versions with low halves zero, 0 to 39 modes from the tables' formats and some
of neither, a mask of formats the device rejects, the mode heap present or not, the heap filled or not; ReadCaps again onto filled words; the four
slots; then Clear, Reset, the destructor, the deleting destructor, the base's destructor, or the base's deleting destructor with 0 and 1. Adapters (40):
construction, Init none to twice, the three slots, then Clear, the destructor, the deleting destructor (0 on the object, 1 on one made on the heap), or
the base's slot 0, destructor and deleting destructor. Adapter lists (30): construction, Init none to three times, Count and At, then Clear, the
destructor, the deleting destructor, or the base's. The factory (16): construction, Start, slot 0, slot 4 with an object or null, slot 3 (heap present,
absent, allocation failing), Stop, the destructor. Tables: 0x000EC2B0 on garbage three times; 0x000EC670 on garbled live tables. The start (12): the
saved globals made and saved by each side's code, tables garbled, then the start, and the stop or not, on the original's static factory and the
recovered one.

Compared: the call logs (allocations by size, frees, pushes with their heaps, raises, every Direct3D call with its arguments), every word of each object
(vftables through a map of each recovered class's to the original's, the adapter pointer and the texts' data as where they point, the names as wide
strings, the modes array word by word), both tables in their 44 known members and the render table's seven (function addresses through a map of the
recovered functions to the original addresses), the factory handed back.

Not compared: the matrix table's 60 slots the host table does not have; the device's own words (gfxdev's suite); the flag slot 4 hands the object's
slot 0 (the original passes 1, the recovered slot takes none).

## Host test

`tests/native/test_grfactory.cpp` (x64): formats, caps to flags, the start through doubles (tables, one adapter, names, two of four modes kept with their
flags, the device flags), the stop, then the graphics layer's Init on the real factory with the caps call failing: the real device made, its creator
the factory, its matrix device handed out. Host tests (x64): 40 / 40 pass.

After the shared edits: grlayer's mutants rerun, **191 / 191 detected** (193 less the two dropped); `--suite all --no-mutants`: 46 suites, no
difference, every entry reached. `tools/verify.py` over the whole project shows 8 MISSING, none of them this work's and none in a file or type it touched (not rerun on the bare
base): AtDataRegistry::Load and ::Read (0x00068880, 0x00067D80), whose AtDataRegistryOwner became a class in at/atlayers, and App's GetConfig07,
GetMathConfig, GetDataManagementConfig, GetScriptConfig, GetKinematicsConfig, GetGraphicsConfig (0x0008C680-0x0008C800), whose config types became
typedefs in at/atlayers and at/grlayer: their records' mangled names need updating (PAU to PAV, and the typedefs' names).

## Quirks of the original, kept

* The adapter list makes one adapter, whatever Direct3D has (+8 = 1); Direct3DCreate8's result is +4.
* The adapter's Clear frees the names but leaves the two texts over them pointing at the freed blocks (the base's slot 0, which releases them, is
  overridden). Its destructor calls Clear directly.
* GetAdapterIdentifier's and EnumAdapterModes' results are not looked at.
* A failed caps call leaves the device info as it was cleared, and the adapter keeps it unenumerated.
* Modes the device does not take, or of an unknown format, are not kept; the array keeps its full size, the unkept constructed modes at its end.
* ReadCaps ORs into what is there; DevCaps bit 0x80000 sets both 0x1 and 0x80; vertex processing bit 0x10 sets 0x10 and 0x20; the raster, Z, alpha
  and blend flags are written twice, to +0x18-+0x28 and again to +0x2C-+0x3C; an extents adjust that is not 0 (a NaN included) sets 0x80 in both
  raster words.
* The names are widened with their sign (movsx): a byte 0x80 or above becomes 0xFF80 and up.
* The start restores the saved globals before filling the tables: the render table's zero matrix part is first copied over the matrix table.
* Slot 3 writes the creator even when the allocation failed (after the fatal raise).

## Shared edits and requests

* include/at/AtGraphicsLayer.h (grlayer's): AtDisplayMode removed (now AtDisplayDevice.h's), the factory class moved to AtGraphicsFactory.h (same
  slots and names, plus constructor, destructor, Start, Stop and `m_adapters`); includes both headers. src/at/AtGraphicsLayer.cpp: the display mode's
  constructor removed. src/at/AtGraphicsLayerSeams.cpp: the start, stop and slots 0, 3, 4 removed (now recovered). config/functions.d/grlayer.toml: the
  0x00043480 record removed. tests/oracle/suites/grlayer.py: two mutants dropped (reason there).
* include/at/AtDisplayDevice.h (gfxdev's): `AtDisplayMode::Set` declared (0x00043420), additive.
* tests/native/test_app.cpp and test_atap.cpp: the expected stops.
* platform/host_d3d.cpp is new port code, linked into test_app by cmake/subsystems/grfactory.cmake (as atap links host_sdk.cpp).
* Request: the texture work's three classes (gfxdev's notes) are now what the real start-up needs next.

## Mistakes

* My first oracle copied set-up adapters byte for byte to test deleting destructors, so two objects owned the same blocks and one was freed twice; the
  heap corrupted and the run hung (the first runner's `--mutate` timed out on it). The deleting cases now use an object made on the heap.
* My first slot 4 double logged the object's address, which differs by side.
* Heredoc patches were refused by the sandbox as too complex; patches since are Python files written with the write tool.
* My first count of the factory's closure (31) missed the vftables' slots and the deleting destructors: it came to 44 records and 4 symbols, the whole
  budget, so the later layers were not started.
