# Start-up order (reference build knd-xbox-usa-5454008a-v1)

Read from the disassembly. "Recovered" means in `src/`, with the results in docs/PROGRESS.md. Everything else is unrecovered, and names in quotes are guesses from strings.

## Entry to main

| Address | What | State |
| --- | --- | --- |
| 0x00085375 | XAPI entry: computes TLS size, creates the main thread | SDK code |
| 0x00085301 | Main thread: C library start-up, then `main(0, 0, 0)` at 0x00011030 | SDK code |
| 0x00011030 | Game `main`: on the static `App` object at 0x002BF310, calls init (0x0008E660), a second step (0x0008EBE0), then 0x0008ED60 | not recovered |

`App` has RTTI, vftable 0x0027AE44, 69 slots. It derives through the `AtAp*Application` chain.

## App init, 0x0008E660

In order, for what is shown so far:

1. Memory report and heap set-up (0x00141500: slot 5), then 0x000703B0, 0x00083490, 0x00143DD0.
2. Settings at +0x654C: constructed (0x000989D0: recovered), then `app.ini` (0x00098A50: recovered, called at 0x0008E72D), then the command line (0x00098410, called at 0x0008E758), so the command line overrides the file.
3. 0x0008CE00 (calls 0x00031590, which sets the stream heap: recovered), 0x0008DBA0, and 0x0008E240, which names `DataXbox\UI\DirtyDsk\loader`.
4. **Mount `AppInit`** through the mount manager (0x000766E0: recovered). If the flag at +0x6574 is set, a list file `AppInit.txt` is opened first (0x00076990: recovered).
5. 0x001416A0, 0x0008CFB0, slot calls on the object at +0x65B8, 0x00090210, then 0x0008DC70, 0x0008DDF0, 0x0008DEA0, 0x00139F10, 0x00097EB0.
6. **Unmount `AppInit`** (0x00076830: recovered).
7. The level manager (0x003B3570) is told the starting level, from +0x6550, through 0x0008E420; the first level's name is copied in.
8. Script and game state (0x0012EEA0, 0x0008E4E0, 0x0008C450, 0x0008CB30, and the object at 0x003B3418 by name lookups).
9. Mount `Persist` and read it (0x00141E10, 0x00142040, in a loop until done), unmount.

The level manager's owner, 0x0013EA90 (vftable near 0x0028DB00), creates it and passes `Levels.txt`. When that runs relative to these steps is not established.

## Main loop, 0x0008ED60

Slot 6 of `App`. Mostly teardown, by its calls: frees many subsystems in sequence. So the loop itself is reached through the second step or the virtual calls in 0x0008ED7C-0x0008EDEF. Not read in detail yet.

## Which archive is mounted when

From the callers of 0x000766E0: 0x0008E78C (`AppInit`), 0x0008EAEB (`Persist`), 0x0009A3FE, 0x0009AA79, 0x0009B48B, 0x0009E4E8. The name strings near those are `GameFramework`, `Level_%s`, `MainMenuFramework` and `Levels`, as list-file names. Not all confirmed.

`AtIoMountManager::Mount` builds `JamFiles\XBox\<dir\><name>.JAM`, asks the device whether it exists, and silently does nothing if not, so **a missing archive shows up only later, as a missing file**.

## The loader start-up waits on

At 0x0008EB33 start-up configures a loader held at App+0x6D48, then calls its `Update(30)` (0x00142040) in a loop until it returns non-zero. Each call spends up to 30 milliseconds on: opening the `.MNG` file, reading its tokens, then running queued items one at a time. The base class is recovered as `GmManifestLoader`. On the disc, `LOADER.MNG` files list the sound and graphics databases of a level; which file this first wait reads has not been traced.

## The memory system

`AtMemorySystem` (vftable 0x00286694) and its derived class (vftable 0x00284840) are recovered. Startup, slot 2, calls Initialize, which allocates the region, starts the manager, creates the default heap, default size 0x10000, and calls what is probably `XInitDevices`; then sets the instance and the default heap, and pushes it. The object is a member, at +4, of the class whose constructor is at 0x00141840: not recovered, and neither is what passes the sizes. The application's own four heaps come later, from 0x0008C860, which reports `***Mem For Heaps` and reads each heap's size and partner from +0x64A4: -2 joins the default heap's group, -1 none, otherwise another of the four.

## Corrections from recovering App (2026-11-18)

* `App` RTTI: App : GmApp : AtApAtlasApplication, AtApUserInterfaceApplication, AtApCinemaApplication, AtApSceneApplication, AtApVideoPlayerApplication,
  AtApSoundApplication, AtApControllerApplication, AtApGraphicsApplication, AtApKinematicsApplication, AtApScriptApplication, AtApDataManagementApplication,
  AtApMathApplication, AtApInputOutputApplication, AtApNetworkApplication, AtApEventApplication, AtApBaseApplication. Constructor 0x0008E5B0, destructor 0x0008F130.
* Step 1: 0x00141500 is GmApp's start-up: the memory report, the audio options' defaults, then the AtAp layers' start-ups in a chain (0x0009C280 atlas, then 0x0009C3C0,
  each checking its own running flag, calling the next, asking App for its config by a slot, and initialising its layer object). 0x000703B0 makes the small-C scripts'
  name map; 0x00083490 and 0x00143DD0 store the first heap.
* Step 3: 0x0008CE00 makes the derived mount manager at App+0x6914 (0x00140350, vftable 0x0028DBA0), with App as its owner and App+0x31C as its context, and the history
  logs at App+0x68F0, and sets the stream heap. 0x0008DBA0 sets 640 by 480. 0x0008E240 loads `DataXbox\UI\DirtyDsk\loader` through App+0x5F90's slot 29, with the heap
  manager's override set to App's first heap. Init then gives the mount manager the App.ini data folder (slot 4) before reading the command line.
* Step 5: 0x001416A0 reads two things from the matrix device; 0x0008CFB0 is unrecovered and large; App+0x65B8 is FaAcPlayerGameStateData (slots 1, 3; slot 5 gets the
  Generic registry); 0x0008DC70 makes the three frameworks; 0x0008DDF0 the table of moves between them; 0x0008DEA0 loads `Design\Registry\Generic.ADR` into App+0x6950 and
  `Library.ADD` into App+0x6B40, each if present; 0x00139F10 reads the saved sound volumes; 0x00097EB0 is not identified.
* Step 7: 0x0008E420 is `GmLevelManager::FindLevel`, comparing eight characters.
* Step 8: the "SName" and "Title" strings go to 0x003050E8 and 0x003050F0; the script system (0x003B3418) is initialised and its callbacks registered (13, 5 for
  "Control", 31 for "SaveData"); both modules are enabled; a "Game" save type of 0xF8 bytes is registered with the space manager; the first framework is entered.
* Step 9: the loader is the level manifest on `PERSIST.MNG` (from the manifest work).
* **The main loop is 0x0008EBE0**, Init's second step, not 0x0008ED60. Each frame: the device's slot 6, the clock, App+0x692C's update, the scripts until done, a move
  between frameworks by the table, the framework's slots 3 and 2, then App's slot 39. 0x0008ED60 is App's slot 6, Shutdown, ending in a tail jump to SetDefaults.
