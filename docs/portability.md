# Portability rules (native targets: Windows x86-64, Linux x86-64 / Steam Deck, Android ARM64)

Applied from the first recovered unit:

* Recovered game code includes no Xbox, Win32, D3D, DirectSound or Android headers. Platform services go behind
  small interfaces in `platform/` (none exist yet — added only when a recovered caller needs one).
* Original 32-bit structure layouts are documented with `static_assert`s that run on x86 builds only. Host builds
  keep the logic, not the layout: pointers are 8 bytes on 64-bit hosts, so original binary data is **never** cast
  to these structures — it is parsed field by field with explicit sizes and byte order.
* Never store host pointers in 32-bit integers; never assume page size or fixed addresses.
* x87 vs SSE: the reference build uses x87 (`fld` seen in game code). Any float-heavy recovered code needs
  determinism tests before host use; the matching build uses `/arch:IA32`.
* Behaviour quirks of the original are preserved and tested (e.g. `GmLinkedListIterator::Prev()` dereferences a
  null current node; `At()` with an empty list and index <= 0 dereferences null). A port-time hardening must be an
  explicit, separately tested change, never a silent "fix" in recovered source. Also preserved:
  `GmLinkedList::Append` does not check `Allocate`, so more than 4000 live list memberships writes through null.
  In the JAM reader: paths go into fixed buffers unchecked, `AtIoFastAlignedJamFile::Seek` adds the current position when it
  moves the archive file (harmless to reads, which seek for themselves), and a read failure in `vInit` reports through `AtFatal`
  and parses what is there if that returns. `platform/fatal.cpp` stops, so a host build does not reach that last path.
  Static capacity limits of this kind (4000 list nodes; 2200 entries in the sorted container's pool, 0x000763B0) are
  candidates for a port-time diagnostic.
* ARM64 compile tests should start as soon as there is more than containers code (alignment, intrinsics).

File access: recovered code reaches files only through `AtIoFileSystem` and `AtIoFile`. `platform/host_file_system.cpp` implements them over
stdio, so that is the seam for Linux and Android (Android's storage access will need its own implementation). It uses only standard C++ so far,
but has only been built and run on Windows x64. Files over 2 GB are out of its scope, as `long` offsets on Windows are 32 bits; no game file is
that large. `platform/fatal.cpp` stands in for the unrecovered heap and error helpers.

Text and numbers: the tokenizer is why this matters most, because its numbers become game data.

* Fractions: the original evaluates on the x87 and rounds only on storing. The source does the arithmetic in `double` and rounds once to `float`. Tested equal to the original on
  every text file on an SSE x64 build, and the original gives the same result under the x87 precisions tried. It is not proven for every possible input: a sum spanning more than 53
  bits could round differently, which needs a fraction far longer than any in these files.
* Writing reals: a modern `printf` rounds ties differently from the original's C library, and keeps the sign of negative zero. `AtFormatReal` reproduces the original from 17
  significant digits, using only `%.16e` from the host (`src/at/AtIoFormatHost.cpp`). `1.#INF00` and `1.#QNAN0` are the original's spellings. The sign of a NaN is read assuming
  little-endian floats.
* `char` is unsigned on Android ARM64: the hash and the keyword comparison cast to `signed char` explicitly. Only ASCII names have been exercised.
* Quirks kept, for port hardening later as separate tested changes: `PutReal` can overflow its 0x20-byte buffer, a keyword over 0x3F characters and a string over 0xFF bytes write
  past their fields after reporting, and `AtCopyName9` reads eight bytes whatever the name's length.

Sizes copied from the original: a literal `* 4` or `0x10` in the disassembly can mean the size of a pointer. `GmLevelManager::Load` had exactly this and overflowed on x64 (docs/verification.md). When a recovered allocation holds pointers or
objects with pointers, write it with `sizeof`. The AddressSanitizer build is the check: configure with `-DCMAKE_CXX_FLAGS="/fsanitize=address /Zi /EHsc" -DCMAKE_TRY_COMPILE_CONFIGURATION=Release`, build Release. From Git Bash set `MSYS_NO_PATHCONV=1` first.

File names: the game asks for `App.INI` and the disc has `app.ini`; it asks for `Levels.txt` and the archive has `LEVELS.TXT`. Xbox file systems ignore case, Windows does too, Linux and Android do not. `AtIoHostFileSystem` will need to fold case there, and it is not tested. The archive layer already compares without case.

Native build today: `CMakeLists.txt` builds `knd_gm` and `knd_at` (`src/`) and the semantic tests for x64 with the VS 2022
generator. Ninja is not installed; Linux/Android builds are untested. The 32-bit oracle target is Windows-only analysis
tooling and is excluded from x64 builds.

* Allocator (`src/at/AtHeap.cpp`): a block's header holds pointers, so every offset goes through `AtHeapBlock`. On a 64-bit host `AtHeap::Init` raises the alignment shift to at least 3, which changes nothing on x86. Sizes keep the original's 24 bits, so a heap of 16 MB or more does not work, on any host, until that is changed deliberately. `AtHeapManager::s_instance` is not yet set by any port code.

* Global new and delete: the original replaces them with the heap stack. Recovered code does not, since the host's library shares the process and would then allocate from a stack nothing has set up. `AtHeapNew` and `AtHeapDelete` are the original's two, under other names. How recovered classes come to use them is a decision for the port, and not yet made. `AtPlatformAllocateRegion`, `AtPlatformFreeRegion` and `AtPlatformInitDevices` are the seams; `platform/fatal.cpp` has host versions.

* **x87 precision, established for the main thread: 53 bits.** Read from the binary, not observed on hardware: the thread start at 0x00085301 calls the C library's initialiser 0x00088CF4, which calls through the pointer at 0x002B81E8 to 0x00125BFF, which, if the word at its first argument's place is not zero, calls 0x00129288: `_controlfp(0x10000, 0x30000)`, which is `_PC_53` under `_MCW_PC`. No argument is pushed, so that word is the return address into 0x00085352's caller, which is never zero. Not established: other threads, which start from whatever the kernel gives them, and whether later code changes the setting. No code in the XBE's game range was found changing it, by a search for `push 0x30000`, but `fldcw` in hand-written or library code has not been read one by one.
* So recovered source computes in `double` where the original keeps a value in a register, rounds to `float` where it stores, and on SSE gives the same bits: `test_at_vector` shows that against a hash of the original's results.
* Sine and cosine: x87's `fsin` and `fcos` return more bits than a double holds. On x86 `src/at/AtMathHost.cpp` uses them. Elsewhere it uses the host's library. Over 2,000,000 angles within +-7 radians the x87 and library results rounded to the same float every time, the largest double difference being 1.11e-16; but that is a host's library, not a promise, and a rotation's rounding can differ. `1 - cos(angle)` near an angle of 0 is computed as `2 sin^2(angle / 2)` on x64, more exact than the original, so a rotation by a small angle may differ from the original in its last bits. At 2^63 and over, `fsin` returns its argument, unlike a library: such angles are not handled.

## Copying a material database can run past its lists

`AtMaterial::Copy` packs each material's range of stages and textures after the last one, into lists sized as the source's. In a file, materials may share slots, so the ranges can add up to more than the list holds. Copying such a file writes past the end of the heap block. `AtMaterialDatabase::CopyFrom` copies the sizes and nothing checks them. 21 of the oracle's copies did this on real files. Whether the game ever copies those databases is not established. The recovered source keeps this as it is; the port should make it a separate, tested change.

## Array counts written by hand

`AtShader` and the scene write out the count ahead of an array by hand (`NewArray`, `src/at/AtSceneArray.h`), since a heap here may answer null and a modern `new[]` is assumed not to. The count takes one `size_t` before the elements on x64, 4 bytes on the Xbox, with the elements still aligned.

## Writing past a shader's lists

`m_transforms` holds eight and is not bounds checked; `TextureStage[n]` over eight reports and, if the report returns, reads on over the fields after it. Kept. Compared only with values that stay in the object on x86. On x64 this is undefined behaviour, and hardening belongs in a separate, tested change.

## Fields read by offset in scene nodes

`AtSceneActorSharedCoordSys` (+0x90) and `AtSceneActorJoints` (+0xA0) are seams, because the base node reads fields of classes derived from it, by offset. They must become real members when the actors are recovered.

## A thiscall pointer that became a plain one

`AtConstructEach` (0x00011000) takes a thiscall pointer in the original and a plain function pointer here, since `__thiscall` does not exist on a 64-bit or GCC build.

## Find on a cleared name map divides by zero

The hash divides by the table's size, which a cleared map has as zero. The original does the same. Any new code calling `Find` on a cleared map crashes, on this hardware and on the Xbox.
