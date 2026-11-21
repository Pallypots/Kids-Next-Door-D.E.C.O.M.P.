# Worklog: the group, water, lens flare and sound classes of a scene node, and the walks into a space graph (2026-11-19)

For the integrator: this holds what belongs in docs/formats.md, docs/verification.md and docs/portability.md. State of this work: Semantic-tested. A host MATCH from `tools/verify.py` says nothing about the original compiler.

## First: no real file uses these classes

None of the 652 distinct `.ASN` texts names GroupActor, LodGroupActor, SwitchGroupActor, WaterActor, LensFlareActor or SoundNode. **Everything here rests on written text and direct calls.** The real files still load in the oracle and in `test_actor`, and show only that nothing changed for the classes they have. The walks are reached by the game right after loading a scene list (callers of 0x0002ED70), so they matter to every real level; their oracle uses written scenes.

## What a real build now does

Every class in `AtSceneActorKind` is real: `AtSceneActorsSize/Construct/Destroy` answer all nine, and `AtSceneUnrecoveredActors*` are gone (from `AtScene.h`, `src/at/AtSceneActorsHost.cpp`, which keeps only DataLink, `tests/native/test_asn.cpp` and the harness). A scene with any of these classes loads and clears. What still stops with a diagnostic: the water drawer (slots 2 and 3 of the owner's drawer maker, `AtSceneParticleDrawerMaker::NewWater/ReleaseWater`, which a running game's maker must override), the sound emitter's slots 25 and 15 (`AtSceneSoundSetVelocity/Play` in `src/at/AtSceneNodesHost.cpp`), and every running-game slot of the six classes.

`AtScene::InsertInto` and `RemoveFrom` are real: their stubs in `src/gm/GmLevelManifestHost.cpp` are deleted. `test_manifest` and `oracle_manifest` still supply their own; the walks are in a file of their own, `src/at/AtSceneWalk.cpp`, so that works.

## Recovered

49 functions, 6 vftables, records in `config/functions.d/scenenodes.toml`; `include/at/AtSceneNodes.h`, `src/at/AtSceneGroup.cpp`, `AtSceneEffectNodes.cpp`, `AtSceneWalk.cpp`, `AtSceneLoaderWalk.cpp`.

* GroupActor, vftable 0x00276870, 0xA8: ctor 0x0004D520, ReadBody 0x0004D600 (word Actor), deleter 0x0003B980, Read 0x0004D410, Clear 0x0004D7E0, dtor 0x0004DA30.
* LodGroupActor, 0x00276920, 0xB0: 0x0004DB60, ReadBody 0x0004DC10 (Lod), Insert 0x0004DAC0, 0x0003B9E0, Read 0x0004DA80 (a jump, also the switch group's slot), Clear 0x0004DE30, dtor 0x0004E100.
* SwitchGroupActor, 0x002769D0, 0xAC: 0x0004E300, ReadBody 0x0004E1D0 (Switch), 0x0003BA40, dtor 0x0004E180. Its Clear is the group's.
* WaterActor, 0x00276A80, 0xB8: 0x0004E4A0, ReadBody 0x0004E780 (WaterPatch with PositionRect, TxtCoordRect, Neighbors, Mask; MatAssignmentOffset, PlaybackRate, DampingFactor, GenerationFactor), 0x0003BAA0, Read 0x0004E4F0, Clear 0x0004E570, dtor 0x0004E6F0.
* LensFlareActor, 0x00276B30, 0x13C: 0x0004EBE0, ReadBody 0x0004ECE0 (LensFlare, LineOfSightTestEnable/Disable), 0x0003BB00, Read 0x0004EBA0, Clear 0x0004ECA0, dtor 0x0004EF00. It holds an `AtSegmentQuery` at +0xA0.
* SoundNode, 0x00276BE0, 0x70, class 16 (the harness said 15): 0x0004EF90, ReadBody 0x0004F0D0 (Emitter, PlaySegment), Read 0x0004EFB0, Clear 0x0004F0B0. Its deleter is the modifier node's 0x0003CD00, folded.
* `AtCoordSys::MarkChildrenDirty` 0x00032310, `WorldChanged` 0x00032DF0, `SetWorldFrom` 0x00032E50, `SetWorld` 0x00032EF0: what a sound node does to its emitter.
* The walks: `AtSceneDatabaseLoader::InsertInto` 0x0002ED70 and `RemoveFrom` 0x0002EDD0; `AtScene::InsertInto` 0x0003E860, `RemoveFrom` 0x0003E890, and the eight they call: `AddDrawing` 0x0003D920 / `RemoveDrawing` 0x0003DBB0 (kind 2, actors), `AddCollision` 0x0003DDB0 / `RemoveCollision` 0x0003E070 (kind 1, plain nodes and actors), `AddLineSegIntersect` 0x0003E2B0 / `RemoveLineSegIntersect` 0x0003E580 (kind 3), `AddModification` 0x0003E7C0 / `RemoveModification` 0x0003E810 (kind 0, modifier nodes).

Closure scripts: `analysis-local/scratch/scenenodes/closure.py` and `vt.py` in the worktree, with the fatal path cut. Load and clear slots of the six classes come to 37 functions, the walks to 12.

## Not recovered

* The running-game slots of the six classes: they stop with a diagnostic and are recorded only as symbols. **Nine of them only pass a call on to the children or return a member, and need nothing outside the scene**: group slots 34 (0x0004D510), 38 (0x0004D430), 42 (0x0004D480); level-of-detail slot 38 (0x0004DA90); switch slots 34 (0x0004E2D0), 38 (0x0004E210), 42 (0x0004E250); water slot 34 (0x0004E460, returns +0x74); lens flare slot 34 (0x0004EEF0, returns the flare's +0x08). Also the actor base's 16, 17, 18 (0x00055CA0, 0x00055CC0, 0x00055C80) and the node's 16, 17, 18 (0x0004A640, 0x0004A6A0, 0x0004A5E0), which only set the motion state and sphere. **I did not recover them**, because `AtSceneActor` and `AtSceneNode` declare those slots with no arguments and no return (`void Slot34()`, `void Slot38()`, ...), and the originals take two arguments (38, 42), one (16 to 18) or return a pointer (34). Recovering them means retyping those shared declarations, every override (model, jointed, particle, the harness's `TestActor`) and the mangled names in `actor.toml` and `particle.toml`. That is a request for the integrator: once the base declares `void* Slot34()`, `void Slot38(void* context, AtCoordSys* parent)`, `void Slot42(void* context, AtCoordSys* parent)` (a null parent means the node's own slot 5), the nine are a few lines each.
* The water drawer and what slot 1 does with the patches; slot 0. The emitter's slots 15 and 25 (25 at 0x0011F190 only stores a vector at +0x34; 15 at 0x0011F010 plays). Both are the aod work's class, `AtAudioEmitter`, which does not declare slots 24 and 25 or the arguments of 15.

## Oracle

`oracle_scenenodes` shares `oracle_asn.cpp`. **Stand-ins on both sides, the same: the water drawers, the lens flares (values made from names), the sound owner's emitters (real coordinate systems with a parent for names beginning Q, and a child each; on the original's side with a harness vftable whose slots 15 and 25 log) and segments.** Space graphs are real on both sides, each read by its own reader.

Compared: every word of each class, children by place, distances by bits, the embedded query by word with its interior pointers by offset, drawer by id, emitter by place; the emitters' coordinate systems with their parents and children; the two statics (patches left, segments pending); the drawer's arguments, the patches by hash; the log of matrix calls, heap, emitter slots. For the walks: every link and item of the graph by place and every node's four links, after inserting twice, removing twice, inserting into a second graph (nothing already in one goes in another), and destroying the scenes while in.

Cases: written text for every word and error path of each class with several headers; groups of groups and shared chains; ctor over two fills, clearing stained words twice, the complete destructor and what it leaves, the deleter freeing and not; Lod insertion over orders, equal distances and NaNs; Read and ReadBody directly where a scene cannot reach (no heap; a lens flare not loaded over a radius; Emitter with no sound owner); the four coordinate functions over eight shapes; walks over mixed scenes and each class alone, directly and through two nested loaders; no heap and no memory for the groups' arrays, the patches and pending segments, refused from each push in turn.

The harness change `oracle_asn.cpp` (shared with asn, actor, particle): the doubles of the six classes are gone and they are real; the class words and sound state are compared in every scene comparison; `MakeRotation` and `ScaleAxes` matrix doubles; a per-side count of pushes to let through before refusing a heap (`g_refuse_skip`); a count of texts and crashes per section. `python -I tools/oracle_run.py --no-mutants`: all 45 suites, no difference.

### Limits

* After a crash only logs are compared. Per section: groups 37 of 215 texts crash on both sides, water 12 of 97, lens flares 4 of 60, sound 7 of 76, shortages 15 of 24. These are the error paths that read through a null after the report, which in the game would not return.
* A second water actor with no WaterPatch is handed the first's freed patches and frees them again (a quirk, below). The harness cannot survive a double free, so no case has it; each written water actor that follows another has patches of its own.
* Group children are only plain models: a jointed model's slots 32 and 33 are not recovered.
* The emitter's slots are logged, not run.

## Quirks of the original worth keeping

* **A water actor's patches are a static (0x002C7E14), freed by Read and not cleared.** A water actor with no WaterPatch after one with patches hands the freed block to its drawer and frees it again.
* A water actor's Clear clears only its own fields: not the actor's, which only its destructor's base clears. So clearing a scene's water leaves its links and coordinate system until destruction.
* PlaySegment words wait in a static list (0x002C7E18) shared by all sound nodes, until that node's Read plays them, last first, and frees them. With no emitter they are freed unplayed. Read sets bit 0x800 on every sound node.
* Emitter on a sound node with no sound owner (owner's +0x14 null) changes nothing and reports nothing; PlaySegment then keeps a null segment.
* The start integer of PlaySegment is read and kept, and never used: the play passes 0.
* Each play first puts the emitter where the node is (its coordinate system's world matrix, or the identity at the origin), then sets a zero velocity.
* A group's first list makes room for n; a later list fills from the start again, checks only its own count against the room, and leaves the earlier children sharing.
* A level-of-detail group's later list adds to the earlier one with no check against the room: past it, it writes beyond both arrays.
* A list of none, `[0] {`, leaves its right curly unread: the body takes it as its own end. A count with no list after it pushes the token back.
* Distances are kept squared; equal ones go before, and a distance that does not compare goes past every one it does not compare with.
* A missing child is reported and then read through the null; a child already sharing is reported and shared again.
* The walks take the actor arrays in the order model, jointed, particle, water, lens flare, group, level-of-detail, switch: the last two not in the scene's field order. Sound nodes and the arrays of unknown classes are in no walk. An actor sharing a coordinate system is in no kind; an actor already linked in any graph is not put in another.
* A lens flare's radii are 0.0001 from construction, Read and Clear; a BoundingSphere in its text wins.
* `AtCoordSys::WorldChanged` makes the local matrix from the world one and the parent's +0x18 after the parent's slot 6, which writes into +0x18. With the names the asn work gave (+0x14 local, +0x18 world), slot 6 writes the inverse into "world". One of the two names may be the other way round; I kept the offsets and the names as they are.

## Portability notes

* `AtNameTableOwner::m_unknown14` is a pointer now, the sound owner. It was `int`, which cannot hold one on x64.
* The patches and the pending list are host pointers in statics; fixed-width fields otherwise.

## Shared edits

Additive unless said:

* `include/at/AtScene.h`: the eight walk helpers declared; `AtSceneUnrecoveredActors*` removed; comments.
* `include/at/AtSceneDatabaseLoader.h`: `InsertInto`, `RemoveFrom`.
* `include/at/AtCoordSys.h`: the four functions.
* `include/at/AtSceneParticle.h`: `AtSceneParticleDrawerMaker` slots 2 and 3, not pure, so existing makers still build.
* `include/at/AtNameTableOwner.h`: **retyped** `m_unknown14` from `int` to `AtNameTableOwner*`, name kept. Every use assigns 0.
* `src/at/AtSceneActor.cpp`: the three array functions answer every kind.
* `src/at/AtSceneActorsHost.cpp`: only DataLink left. `src/gm/GmLevelManifestHost.cpp`: the two scene stubs deleted.
* `tests/native/test_asn.cpp`: its stand-in under LodGroupActor is gone; the text uses the real class.
* `tests/oracle/oracle_asn.cpp`: as above. `kClasses` for the sound node is 16; `kOriginalDtor` for it is 0x0003B910.

## Mistakes along the way

* My first host test expected sound nodes in the walks, against what I had written in the header. They are in none.
* The harness compared emitter blocks it had allocated room for but not made: garbage vftables, and 30,000 differences that were mine. Now only those made.
* The original's graph needs its heaps to be AtHeap-sized blocks: an `int` crashed it in 0x000535A0.
* Python on this machine writes CRLF, and heredocs eat backslashes: patches go in script files.
* **The first mutant run was 255 / 275, and most survivors were a gap of mine:** the main walk scene had no `Node[n]` head before its list of nodes, so it held no plain or modifier node, and nothing about kinds 0, 1 and 3 of plain nodes, or the order of the arrays, was compared. A count of links per kind now shows they are there. The other gaps, closed by direct cases: a group's count over a no-list read, the water's class after Clear, the lens flare's destructor over uncleared words. Six mutants dropped as provably equivalent, with reasons at the head of `tests/oracle/suites/scenenodes.py`.

## Result

`python -I tools/oracle_run.py --suite scenenodes --jobs 2`:

```
NO DIFFERENCE OBSERVED: 652 files, 15622 nodes seen, 6538 actors; 8222674 comparisons, 0 differences
entries reached in every run: 49 / 49
(unmodified copy: positive control) passes
mutants detected: 273 / 273
```

`python -I tools/oracle_run.py --no-mutants`: all 45 suites, no difference, after the last change to the shared harness. The asn, actor and particle mutant runs were **not** repeated after the harness change; their no-mutant runs pass.

`ctest` on x64: 40 of 40, `test_scenenodes` and `test_asn` included (`test_actor` and `test_particle` from the run before the last change to the host test alone).

`tools/verify.py` on the 49 functions: 4 MATCH, 45 SIZE, none MISSING or UNRESOLVED; all 6 vftables MATCH. Host compiler, so that says nothing of the original's.
