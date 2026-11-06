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
