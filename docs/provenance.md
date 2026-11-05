# Provenance

Independent reverse engineering of the owner's own retail disc images. No leaked source, symbols, SDK packages or
confidential material are used. Names come from the binary itself (RTTI, strings) or are inferred and marked so.
This is **not** a formal clean-room process.

## Owner inputs (C:\KND, never committed)

| File | Size (bytes) | SHA-256 | MD5 | SHA-1 |
| --- | ---: | --- | --- | --- |
| `Codename - Kids Next Door - Operation - V.I.D.E.O.G.A.M.E. (USA).iso` (full disc image) | 7825162240 | `2fb6b0ccdf2b53ff6b6ab5408aeccb72a375b4dab392d56920830c267925db6a` | `c2d200e5276374179092b9e9c66d6633` | `ec3b088208267287fffdf1fca713af7e3c68bd1f` |
| `… (USA).xiso.iso` (XISO) | 1997996032 | `9c45cfd0ba52eb7b4ee8ec0adb29c11fd52b13488dcd31a51f059de4623f3e69` | `5ae5a9870793f82b0f03ac524afcd3c1` | `19e947f2bcf62b450b8a196973833ea86ea28487` |

Hashed 2026-11-05 with `sha256sum`/`md5sum`/`sha1sum` (MSYS2). Inputs were only read, never modified.

* The full image's MD5/SHA-1 equal those in the owner's `info.txt`, which says the XISO was made from it with
  `extract-xiso build-202609111233`. That is a consistency check against the owner's note, **not** an independent
  check against the Redump database (not yet consulted).
* The xemu title page lists title ID `5454008a`; the XBE certificate agrees. Corroboration only.

## Container analysis

| Image | XDVDFS partition base | Root dir sector / size | Volume timestamp (FILETIME) |
| --- | --- | --- | --- |
| full image | `0x18300000` | `0x1A2DAE` / `0x800` | `0x01C5AB6922EF48B0` |
| XISO | `0x0` | `0x108` / `0x60` | `0x01C5AB6922EF48B0` |

Both trees list the same 235 entries with identical sizes (`tools/xdvdfs.py ls`, compared with `diff`).
`/default.xbe` extracted from both is byte-identical: SHA-256 `f5f48a6f…5f96929`. It is the only XBE on the disc.

## Reference build `knd-xbox-usa-5454008a-v1`

(`python -I tools/xbe.py info orig/xiso/default.xbe`)

* Title "Codename: Kids Next Door", title ID `5454008A`, certificate version 1, region `0x1`, media `0x202`.
* XBE timestamp 2005-08-21 19:15:39 UTC; PE timestamp 2005-08-21 19:15:33 UTC; certificate 2005-08-31 19:50:13 UTC.
* Entry point and kernel thunk decode only with the **retail** XOR keys → retail-signed layout.
* Debug path left in the header: `c:\Develop\2x4\Dev\Build\Net\App\xbox_knd_final\xbox_knd_final.exe`.
* All 17 section SHA-1 digests in the header match the section bytes. A patched executable would normally break
  these unless the patcher re-computed them; the RSA header signature is **not** verified (no key), so "unmodified"
  is strongly supported but not proven.
* Library versions (all 1.0.5849): XAPILIB qfe1, XBOXKRNL qfe1, LIBCMT qfe1, D3D8 qfe15, D3DX8 qfe1, XGRAPHC qfe9,
  DSOUND qfe11, XACTENG qfe6.
* 124 kernel imports by ordinal; no non-kernel imports.

Only one revision is known to exist in this workspace. Other releases (PS2, GameCube, other regions) are out of
scope unless the owner decides otherwise; never mix their addresses.

## Tools used

| Tool | Version | Use |
| --- | --- | --- |
| Python | 3.12.10 | all project tools (`python -I`) |
| capstone | 5.0.9 (pip) | x86 disassembly in tools/funcs.py, xdiff.py |
| MSVC | 19.44.35229 (VS 2022 Build Tools 14.44.35207), x86 | host compiler for the verification loop (not original-era) |
| CMake | installed at C:\Program Files\CMake (VS 17 2022 generator) | native host build |
| tools/xdvdfs.py, xbe.py, funcs.py, xcheck.py | this repository | written for this project, tested in tests/ |

Not installed / not used yet: Ghidra (+ XboxDev ghidra-xbe), Java, objdiff, extract-xiso, nxdk.
The installed `matching-decomp` skill was read; its `dk.py` supports only ELF/MIPS relocations (header and `R_MIPS`
table), so it is kept as reference and **not** used on this build. Its principles are adopted in docs/verification.md.
