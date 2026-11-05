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
  Static capacity limits of this kind (4000 list nodes; 2200 entries in the sorted container's pool, 0x000763B0) are
  candidates for a port-time diagnostic.
* ARM64 compile tests should start as soon as there is more than containers code (alignment, intrinsics).

Native build today: `CMakeLists.txt` builds `knd_gm` (`src/gm/`) and the semantic test for x64 with the VS 2022
generator. Ninja is not installed; Linux/Android builds are untested. The 32-bit oracle target is Windows-only analysis
tooling and is excluded from x64 builds.
