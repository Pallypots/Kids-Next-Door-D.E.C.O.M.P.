# Data formats (reference build knd-xbox-usa-5454008a-v1)

JAM directories are recovered from code and validated against every archive (below). The other rows are observations
from file magics and the XBE's strings, not yet parsed. Nothing here is copied from game data beyond identifying bytes. The disc listing is regenerated locally with
`python -I tools/xdvdfs.py ls <image>`.

| Type | Count / total bytes | First bytes | Observation / hypothesis |
| --- | --- | --- | --- |
| `.JAM` (`DataXbox/Jamfiles/Xbox/{,Arenas,Levels,Viewers}`, `UI/…`) | 56 / 958,661,566 | `FSTA` (35) or `JAM2` (21) | Main asset archives; directory layout below. The XBE builds paths with `JamFiles\XBox\%s%s.JAM`. `AppInit.JAM` is 623 MB. |
| `.wba` (`Audio/Music/*`) | 15 / 321,642,496 | `HVSIWBA\0` + u32 1 | Audio bank container; the `HVS` prefix also appears in the XBE string `Intro\HVS`. |
| `.aod` `.aob` `.AOS` `.aes` `.gom` `.agi` `.agm` `.AGD` | small | ASCII: `BankDatabase[2]`, `Bank[1]`, `Segment[46]`, `NameSpace "music…`, `PlayLists[3]`, `MatAssignment[1]`, `Material[1]`, `MaterialDatabase` | Text descriptors with `Name[count] { … }` blocks: music banks/segments/playlists, UI materials. |
| `.sfd` (`FMV/{Attract,Card,Concept,Intro,Weapon}`) | 67 / 708,501,504 | `00 00 01 BA` (MPEG-2 pack) | CRI Sofdec movies; the XBE links CRI SFD/XBOX 1.940 and ADXT/XBOX 9.28. |
| `.xbx` | 3 | `XPR0` | XDK packed resource (save and title images). |
| `.dds`, `.bmp` (`UI/DirtyDsk`) | 10 + 5 | `DDS `, `BM` | Standard formats; "dirty disc" error screens per language. |
| `Sys/XBox/dsstdfx.bin` | 1 / 18,608 | zeros | Name matches the XDK DirectSound standard effects image; unverified. |
| `app.ini` | 1 | `{ DataFolder "DataXbox" }` | Same text block syntax as the descriptors. |

Middleware identified from XBE strings (version strings are quoted from the binary):
CRI ADXT/XBOX Ver.9.28 (Sep 2 2004), CRI SFD/XBOX Ver.1.940, CRI SFH 1.19, CRI SFX 2.08, CRI SUD 0.05
("Copyright (c) 1999-2004 CRI-MW"); IJG libjpeg ("Copyright (C) 1996, Thomas G. Lane"); XDK D3D8/D3DX8/XGRAPHC/
DSOUND/XACTENG/WMADEC/XPP sections. Two strings mention "Havok" (`InitWithHavokOrientation`, `…Position`); this does
**not** establish that a Havok library is linked — unverified.

## JAM archives

Source of the layout: `AtIoJamFileSystem::vInit` (0x0007D770), `AtIoJamInfoHeader::vInit` (0x0007FE80),
`AtIoFastAlignedJamHeader::vInit` (0x0007F750), `AtIoFastJamHeader::vInit` (0x00080450), directory lookups
(0x0007F220, 0x000800A0) and listings (0x0007F9E0, 0x00080710). Those class names are the game's own, from its error
strings. Parser: `tools/jam.py`; synthetic tests: `tests/test_jam.py`.

Validation (`python -I tools/jam.py check <image>`, 2026-11-05): all 56 archives parse, 26,508 files, no file extent
outside its archive, no overlap, no duplicate path, no unexplained JAM2 gap. Every one of those files also reads back through the recovered
source with the CRC-32 Python computed (docs/verification.md). That is strong evidence for the directory
layout, not proof that every field's meaning below is right. Fields not named are not understood.

The token selects the header class: `JAM1` 0x00081020 (not seen on this disc, not implemented), `JAM2` 0x00080390
`AtIoFastJamHeader`, `FSTA` 0x0007F650 `AtIoFastAlignedJamHeader`, `FSTR` 0x0007D190 (subclass of FSTA's, not seen on
this disc; implemented from code only, so it has never been run on a real file).

All integers little-endian.

Info header (first 0x20 bytes of the file):

| Offset | Type | Meaning |
| --- | --- | --- |
| 0x00 | char[4] | token |
| 0x04 | u32 | unknown; stored at AtIoJamInfoHeader+4 |
| 0x08 | u32 | whole header size, read into memory at once |
| 0x0C | char[15] | label (`none`, `safetest`) |
| 0x1C | u16 | name count N |
| 0x1E | u8 | extension count E |

Then, in the header:

| Offset | Size | Content |
| --- | --- | --- |
| 0x20 | 8 N | names, 8 chars, NUL-padded when shorter |
| 0x20 + 8 N | 4 E | extensions, 4 chars including padding |
| 0x20 + 8 N + 4 E | 4 | root: u16 subdirectory count, u16 file count |
| next | | root table |

A directory table is its subdirectory entries followed by its file entries. A subdirectory entry is 8 bytes:

* u32 word: name index in bits 0-11. FSTA and JAM2: subdirectory count in bits 12-20, file count in bits 21-31. FSTR
  swaps those two fields.
* u32 offset of the child table, from the start of the file.

File entries differ by class:

| Class | Size | Fields |
| --- | --- | --- |
| FSTA, FSTR | 12 | u16 name index, u8 extension index, 1 byte (unread here), u32 data offset, u32 size |
| JAM2 | 8 | u16 name index, u8 extension index, 1 byte, u32 record offset |

A JAM2 record starts on a 0x20 boundary: u32 size, u32 size (equal in all 21 archives; whether they can differ, for
example for compressed data, is unknown), padding to 0x20, then the data. The game treats them differently: `AtIoFastJamFile::Open`
0x00082DD0 uses the first as the size that bounds reads and keeps the second separately, while `GetFileInfo` 0x00080BA0 reports the
second. The next record starts at the next 0x20
boundary. FSTA data has no such prefix; alignment rules there have not been established.

Lookup, recovered as source in `src/at/` and compared with the original (docs/verification.md): the requested path is folded
to upper case (0x001261EA, `toupper`), and names and extensions are binary-searched in their sorted tables. A file with no
extension uses extension index 0, which is the empty string. `FindFile` (0x0007F4B0) returns the FSTA `+4` value as the offset
and writes the `+8` value as the size, confirming those two fields.

Two original behaviours a port should know about: `FindDirectory` does not rewind after a path component misses and only
checks the last component, so `FILEA\\ROOT` can succeed when `FILEA` is a name but no directory there; and paths are copied to
fixed 256-byte stack buffers with no length check. Path separator is a backslash.

Contents of the archived files (`.AGD`, `.AGM`, `.AGN`, `.AGT`, `.AFA`, `.AFD`, ... and `.DDS`) are not yet recovered.
Next format work starts from the code that consumes one of these types, never from guessing at the bytes alone.
