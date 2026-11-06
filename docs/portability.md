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
  every text file on an SSE x64 build, and the original gives the same result under both x87 precisions. It is not proven for every possible input: a sum spanning more than 53
  bits could round differently, which needs a fraction far longer than any in these files.
* Writing reals: a modern `printf` rounds ties differently from the original's C library, and keeps the sign of negative zero. `AtFormatReal` reproduces the original from 17
  significant digits, using only `%.16e` from the host (`src/at/AtIoFormatHost.cpp`). `1.#INF00` and `1.#QNAN0` are the original's spellings. The sign of a NaN is read assuming
  little-endian floats.
* `char` is unsigned on Android ARM64: the hash and the keyword comparison cast to `signed char` explicitly. Only ASCII names have been exercised.
* Quirks kept, for port hardening later as separate tested changes: `PutReal` can overflow its 0x20-byte buffer, a keyword over 0x3F characters and a string over 0xFF bytes write
  past their fields after reporting, and `AtCopyName9` reads eight bytes whatever the name's length.

Native build today: `CMakeLists.txt` builds `knd_gm` and `knd_at` (`src/`) and the semantic tests for x64 with the VS 2022
generator. Ninja is not installed; Linux/Android builds are untested. The 32-bit oracle target is Windows-only analysis
tooling and is excluded from x64 builds.
