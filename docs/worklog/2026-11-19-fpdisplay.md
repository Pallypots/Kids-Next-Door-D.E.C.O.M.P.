# Worklog: the Flash player's display manager, AtFpDisplayManager (2026-11-19)

State: Semantic-tested. Host compiler only, so no byte-match claim. `python -I tools/oracle_run.py --suite fpdisplay --jobs 2`.

## Result

* `fpdisplay`: `NO DIFFERENCE OBSERVED: 20 archives, 20 .AFA and .AFP; 1062 script blocks, 19361 instructions; 21844 runs, 20745 of them cut at an instruction, 4 crashed alike, 0 stopped alike as hung; 44741780 comparisons, 0 differences`
* Entries reached: 66 / 66 (armed with `--entries`). Positive control (`--mutate`) fails as it should.
* Mutants detected: 302 / 302 (10 of them by a crash, mostly the heap's own check catching a list left inconsistent). Twelve dropped as equivalent, with their reasons at the head of the suite. The first run left 76 survivors and the second 12; nearly all were gaps in the harness, now closed (see My mistakes).
* `test_fpdisplay` passes on x64. Every oracle suite with no mutants (`--suite all --no-mutants`) (41 suites) shows no difference, asvm, aft and afd among them, whose harnesses are untouched; all 36 host tests pass on x64.
* `tools/verify.py --only` gives 3 MATCH, 4 MISMATCH and 59 SIZE for the 66 functions; no vftable is claimed.

## How far real UI scripts now run

The oracle builds, for every real script block of the 20 user interface archives, a level from the archive's own sprites (.AFP, .XFF) and scripts (.AFA, .AIS): the root sprite node (sprite 0) constructed, named
`_level0` and taken to frame 0 by `GotoFrame`, which places that frame's nodes from the real PlaceObject2 tags. The block then runs on the level's own action script machine with the real display manager behind it,
the original's code on one side and the recovered source on the other.

* All 1062 real blocks run to their end on both sides with no report (no `AtFatal`) and no crash; 19361 instructions; 20466 more runs cut at each instruction. After the whole runs, the trees held 5818 nodes in all (about 5.5 a level: the root and its first frame's), 806 variables in 239 of the runs, 625 actions queued and 458 nodes dead (frames run backwards, removes).
* What a script asks for is answered by recovered code: GetVariable and SetVariable by path ("/a:b", "_level0/a:b", ":b") into real sprite nodes' variable tables, SetTarget, GotoFrame, GotoLabel, GotoFrame2 by number,
  label or "path:frame", Play and Stop, Get/SetProperty, Call (a frame's actions run at once, recursively on the same machine), GetURL2's remote calls with the target's variables, Trace.
* What stops at a seam in a real build: the nodes' virtual slots (Init, Acquire, Update, Replace, Release; so in a real build the first placement of a node stops at `Init`), a placement's matrix (0x00020EA0), a
  level's sounds, what LoadMovie loads (four functions), and the remote functions' game code (registered by the game). In the oracle these are stand-ins, the same on both sides. In particular the stand-in for
  `Update` does nothing, so placed sprites do not advance their own frames: only the root's frame 0 is placed, and a child sprite's frames run only when a script sends it to a frame.

## What the display manager is

```
application +0x004   AtFpDisplayManager, 0x25C bytes: +0x004 levels, +0x008 the levels shown (by number, linked through each level's +0x000), +0x010 loaded, +0x018 unloaded, +0x038 the stack of contexts,
                     +0x21C level count, +0x220 highest level number, +0x248 the environment
contexts             50 entries {context, target}, both sprite nodes, count at +0x194; the machine reads the top itself; ExecuteScripts pushes {target, target} and pops by hand
level +0x00C         the root sprite node ("_levelN"); +0x324 points at it; +0x320 the environment; +0x328 also the count for "instance%d"; +0x340 N
node                 AtFpCharacterNode 0x58: +0x04 link, +0x10 two matrices, +0x14 kind (0 shape, 2 sprite, 3 text), +0x18 character, +0x1C state (1 alive, 2 dead), +0x20 set by a script's SetProperty,
                     +0x24 parent, +0x28 the level's script table (the level's later fields are reached from it by offset), +0x2C frame placed, +0x30 depth, +0x38 colour, +0x4C name, +0x50 ratio, +0x54 heap
sprite node          0xB8: +0x5C children (sorted by depth, unsigned), +0x68 playing, +0x6C frame (from 0), +0x70 sprite, +0x74 pending (placed, not yet children), +0x7C variables
variables            AtFpRfcArgs 0x3C: an array from new[], sorted by name without case, binary searched; an undefined value named "UNDEFINED" for names not found; 10 to start, growing by half (50 to 100)
                     when one slot is left
environment +0x1A0   the queue of frames' actions {script, node}, with a free list; +0x1C4 the dead nodes, newest first
remote calls         application +0x264: a translation callback at +0x08, a registered list from +0x0C {next, function, name}, then a static table at 0x002B1370 (count) / 0x002B1378 (0x18-byte entries, a member
                     function pointer and adjustment, the name at +0x10): one entry, "PreloadMovie"
```

Paths: "/a/b" from the target's root; else each shown level's root matching by its own name first ("_level0/a"); else from the current target (or context, for SetTarget). A name is matched without case and as a
prefix ending at '/' or the end. The first sprite child whose name matches decides, whether or not the rest is found below it.

GotoFrame(n): backwards, children placed after n are removed and the timeline restarts from frame 0. Each frame's tags run in order: place (new, move, replace; flags 1 matrix unless a script has set a property,
2 colour, 4 name, 8 ratio; a new or replaced sprite with no name is named "instance" and the level's count), remove (a child, or a pending node), sound and action (only on frame n itself: the sound plays, the
action is queued). Pending nodes join the children once the frame reached is n - 1 or later, each acquired and updated first.

Properties by the Flash numbering: 0 x, 1 y, 2 xscale, 3 yscale, 4 currentframe (from 0), 5 totalframes, 10 rotation, 11 target, 12 framesloaded (= totalframes), 13 name; 6 to 9 and others undefined. Set: 0, 1,
2, 3 and 10 only.

## Done

66 functions, `at/fpdisplay`, `src/at/AtFpDisplay.cpp`, `include/at/AtFpDisplay.h`:

* `AtFpDisplayManager` (16): GetCurrentFrame 0x0002BF50, GotoFrame 0x0002B970, SetPlaying 0x0002BFC0, StopSounds 0x0002BF20, GotoFrameByLabel 0x0002BF80, GotoFrameByExpression 0x0002CB70, SetTarget 0x0002C3B0,
  LoadMovie 0x0002CFD0, ResolveTarget 0x0002CC90, GetVariable 0x0002C9C0, SetVariable 0x0002CAA0, GetProperty 0x0002C320, SetProperty 0x0002C360, ExecuteScripts(int) 0x0002C150, ExecuteScripts(const char*)
  0x0002C1C0, ResolvePath 0x0002BFF0.
* `AtFpDisplayMgrContextStack` (5): Push 0x00016080, GetCurrentContext 0x000160E0, SetTarget 0x00016130, GetContext 0x00016180, GetTarget 0x000161B0.
* `AtFpRfcTable::Call` 0x0002D9E0. `AtFpRfcArgs` (8): Construct 0x00021620, Destruct 0x00021AD0, Init 0x000216E0, Add 0x00021DD0, Clear 0x00021680, Get 0x00021820, Find 0x000217B0, Insert 0x00021860.
  `AtFpActionScriptValue` SetName 0x00020B60, SetUndefinedNamed 0x00020D20.
* `AtFpActionQueue::Push` 0x000162C0; `AtFlashList` Append 0x00012720, Remove 0x00012750.
* Nodes: constructors 0x00020F70, 0x000210B0, 0x00014300, 0x00021FE0, 0x00030570; SetName 0x00020E10, Root 0x00020D70; ChildAtDepth 0x00021210, AddChild 0x00021130, Kill 0x000213D0, RemoveAllChildren 0x000214A0,
  RemoveChild 0x00021560; sprite GotoFrame 0x00015130, FindLabel 0x00014480, ExecuteFrame 0x000145B0, GotoLabel 0x0002B9A0, ExecuteLabel 0x0002B9C0, FindChild 0x000144D0, TargetPath 0x00014250, GetProperty
  0x00014600, SetProperty 0x00014910, RemoveChildrenAfter 0x00014CA0, FlushPending 0x00014D00, FindPending 0x00014D60, ExecutePlace 0x00014F00, MakeNode 0x00021240.
* `AtFpSprite` Frame 0x00016A60, FrameName 0x00016AA0; `AtFlashScriptTable::RunScript` 0x00013380; `AtVector2` SetNormalized 0x00021F10, NormalizeLength 0x00021F60.

No vftable is claimed: the five node classes' constructors set recovered vftables, but every slot is a seam. Names are inferred but for those the original's messages give (see the header).

## Not recovered (src/at/AtFpDisplaySeams.cpp), each stopping with a diagnostic

* The nodes' virtual slots: base 0x00021080, 0x00020D80, 0x00020FD0, 0x00020D40, 0x00020D50; branch 0x00021540, 0x00030590, 0x000210D0, 0x00021470; sprite 0x000152F0, 0x000143B0, 0x00014370, 0x000153C0,
  0x00014420, 0x00014DB0, 0x00014DE0, 0x000CBF30, 0x00014C40; shape 0x00022280, 0x00022060, 0x00022000, 0x001CC9F0, 0x000220D0, 0x000221D0, 0x00022200, 0x00021FB0; text 0x00030FD0, 0x000305D0, 0x000307A0,
  0x00030F60, 0x000306A0, 0x00030600. Init reaches the graphics device for the matrices (0x00020D80); Acquire the level's sprites (0x00012510, 0x000169B0, 0x00017030); Update is the per-frame tick (0x000153C0,
  0x00014EA0, 0x00015310).
* `AtFpCharacterNode::SetMatrix` 0x00020EA0 (the matrix table's 0x002C60E4 and SetTranslation).
* `AtFlashSoundTable` StopAll 0x00012AA0, Play 0x00012790.
* For LoadMovie: UnloadLevel 0x0002C570, FindLoadedLevel 0x0002C5D0, NewLevel 0x0002CCB0 (which needs the level's Init 0x00011B40), AttachLevel 0x0002C430.

## What the green result does not show

* **Stand-ins for every seam, the same on both sides**, each call logged with its arguments: the nodes' slots (a sprite's Init sets the fields this code reads and initialises its variables with each side's own
  code; Acquire gives it its sprite from the loaded table; Update and Replace only log), a placement's matrix (written into a 4x4 of floats), the matrix table (Get, Set, GetPosition, Identity, SetTranslation over
  those floats), sounds, the LoadMovie seams, the remote functions' translation and handlers (which log what they are given: the target's variables with their names and values), and the clock. So what a real
  Init, Acquire or Update does, and real matrices, are not compared; nor does any placed sprite run its own frames.
* **What is compared**: after the build and after each run, every node of the tree depth first (class, every field above, both matrices), each list both ways, each sprite's variables value by value with names
  and text, the dead nodes, the queued actions, the contexts, the machine's stack, the clock and the random cursor; and the call logs, folded into a hash past half the log's room (at the same point on both sides
  when they agree).
* **Written cases** for what the real files do not reach: a written level (place, move, replace, remove of each kind, sounds, actions, an unknown place type, a remove of a pending node, depths before and after),
  scripts for every frame, label, path, target, property and remote call form, 130 variables, each recovered function called directly (variables with each heap and each allocation failing in turn, the remote
  table's every early return and path, the contexts at counts 0, 1, 2, 49, 50, 51 and 200, the queue, the lists, 20000 vectors), and the manager with no context, a null context or target, or a dead root.
* **Not run**: a path of 0x100 characters or more before a colon (the original copies it past its 0x100-byte buffer; here it is cut to 0xFF), and a remote call name not found of about 107 characters or more (the
  original formats "RFC %s not defined. (%s)" with it into a buffer too small for two, over its return address; the recovered code does not format the message, which nothing reads). A remove tag whose depth is
  neither a child nor pending faults on a null link on both sides; no written frame has one.
* **Crash cases compared up to the crash**: the queue's allocation failing, the variables' each allocation failing, a null context's root.
* A script that loops forever without allocating would hang both sides; a watchdog stops a run after 20 seconds and the oracle counts it. None did.

## Quirks of the original, kept

* ResolveTarget does not check for null: a path naming nothing gives the variables of a null node, 0x7C.
* ExecuteScripts(int) runs the frame given; the machine passes the Flash frame less one. The "path:frame" forms take a frame from 1.
* Currentframe (property 4) is from 0, unlike Flash's.
* The variables grow when one slot is left, not when full; moving up copies values' bytes, text pointers and all, and the slot left is cleared without freeing.
* An insert that finds its name after all (a narrow window of the binary search) copies over the value found.
* The variables' Clear also clears m_heap, so a table cleared must be given its heap again before Init.
* A node's SetName frees its old name and does not null it; a value's SetName does.
* SetVariable returns what happened to be in a register; here 0.
* LoadMovie copies the name to eight bytes and never uses the copy; the level number is atoi of the target past "_level", compared unsigned with the highest.
* The first sprite child whose name matches a path's next part decides the search, even if the rest is not found below it; FindChild matches a name as a prefix that ends at '/' or the end.
* GotoFrame's remove of a depth that is not a child and not pending faults; a place on a depth whose child was placed later than the current frame is skipped.
* Depths are compared unsigned, placed frames signed.
* GetCurrentContext with a count of 0 reads the word before the stack and its first word; with a count past 50, past the entries.

## For docs/formats.md

The block "What the display manager is" above.

## For docs/PROGRESS.md and docs/verification.md

A new suite, `fpdisplay`, subsystem `at/fpdisplay`, cases `tok` (served from `analysis-local/oracle-jam`, as in aft and asvm). The oracle takes `--quick` (no cut runs) and `--trace`, and prints how far real blocks
get (nodes, variables, actions, dead nodes).

## Shared edits

* `include/at/AtFpDisplay.h` rewritten: the classes the machine reached are now real, with their layouts. `AtFpDisplayManager` is 0x25C bytes (was 0x1D0 known); `AtFpRfcArgs` gains its fields, Get, Find, Insert
  and s_heap; `AtFpRfcTable` gains its layout, s_heap, s_count and s_entries; the context stack gains Push, GetCurrentContext, GetContext, GetTarget. Entries stay `void*`, so the asvm suite's harness is untouched.
* `include/at/AtFlashShape.h`: `AtFlashApplication` loses `m_unknown1D4` (the manager now runs to +0x260); `AtFlashEnvironment` splits `m_unknown004` into `m_unknown004`, `m_actions` (+0x1A0), `m_unknown1B8`,
  `m_dead` (+0x1C4), `m_unknown1CC`.
* `include/at/AtFlashLevel.h`: unions add `m_link` over `m_unknown000`, `m_root` over `m_unknown324`, `m_number` over `m_unknown340`; `m_player` is sized by the sprite node, the same 0xB8 bytes on x86. On x64 the old 0xB8 could not hold one.
* `include/at/AtFlashScript.h`: `AtFpActionScriptValue` gains `m_name` in a union with `m_unknown24`, SetName and SetUndefinedNamed; `AtFlashScriptTable` gains RunScript.
* `include/at/AtFlashSprite.h`: `AtFpSprite::FrameRef` gains `m_frameData` in a union with `m_unknown08`; `AtFpSprite` gains Frame and FrameName.
* `include/at/AtEmitter.h`: `AtVector2` gains SetNormalized and NormalizeLength.
* `include/at/AtCoordSys.h` and `src/at/AtCoordSys.cpp`: `AtMatrixTable` gains Get (0x002C61EC) and Set (0x002C61E8), with defaults that stop with a diagnostic, as its others do.
* `config/functions.d/asvm.toml`: the 22 seam symbols of the functions now recovered are removed from its `[symbols]`.
* Nothing renamed.

## My mistakes

* The first mutant run left 76 of 314 alive. Most were gaps in my harness: the written "calls" script hit a Call that stops the script before half of its cases; GetProperty results were only ever left on the stack, which is compared at the end, so each is now traced (and so logged); no direct test compared the constructors; the stand-in for Replace did not do what the original's 0x00020D50 does (kind, character, frame placed), so the paths that depend on it were dead; no node had a heap of its own; vectors exactly at the short length were altered before use; the remote table's direct calls were translated, so the static table was rarely reached; no edge had a second level, a dead target, grandchildren, or a second sprite of the same name. Twelve were equivalent and are dropped with reasons.
* When I spliced new written cases into the oracle, I overwrote my own earlier edit that gives written roots a heap of their own; two mutants survived the third run for that reason alone.
* The oracle called a recovered method (a value's SetName) on the original's side, whose pops the doubles log on the recovered side: a false difference until each side called its own.
* The remote table's direct test did not compare crashes, and a long name not found crashes the original (see Not run); found when the trace showed a crash on one side only.
* My host test expected the variables to grow 10, 60, 120, 220; the original grows by half with at least 50: 10, 60, 110, 165. The raw storage for the variables' undefined value was 0x2C bytes on x64 too, a heap corruption there; sized by pointer width now, and checked by a static_assert in the host test.
* A harness array named `small` met windows.h's macro of that name.
