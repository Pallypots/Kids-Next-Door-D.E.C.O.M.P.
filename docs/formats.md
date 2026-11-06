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

## Text files (tokenizer: src/at/AtIoTextTokenizer.cpp)

Many archived files are plain text, read through one shared tokenizer, not per-format parsers: `.RMB`, `.GOB`, `.GOD`, `.GOV`, `.ADD`, `.AUS`, `.AUD`,
`.AES`, `.AET`, `.AIS`, `Levels.txt` and others. Which file types use it is shown so far only by their first bytes and, for `.RMB` and `Levels.txt`, by the
code that opens them; the other types' readers are not recovered. `tools/jam.py textcases` found 5,804 distinct files that begin as text.

The lexical rules, from 0x00022DF0, are:

| Input | Token |
| --- | --- |
| blanks, tabs, CR, LF between tokens; `//` to end of line | skipped |
| `"..."` | string, at most 255 bytes; may span lines. Backslash escapes (`\n \t \" \\`, capitals too) only if the tokenizer's flag at +0x11C is set. The game text reader sets it while it reads; other readers leave it off, so a path like `"Design\Registry\Actor"` keeps its backslashes |
| decimal, `0x` hex, either with a leading `-` | int, wrapping at 32 bits |
| digits with `.`, or a leading `.`; never an exponent | real |
| letter, then letters, digits, `_` | keyword, case-insensitive, looked up in a table the caller supplies; not in the table gives the error token 1 |
| `! % ^ & * ( ) + = < > \| ? @ ~ [ ] { } , ; : / -` | their own tokens |

**Reals are not the nearest float to their text.** The code adds digit times a running single-precision scale, so `100.001` reads as `0x42C80083` (values in
`tests/native/test_at_tok.cpp`, each asserted against the original). A port must parse them the same way, or positions and tuning values
change in their low bits.

The usual shape is a counted block: `Name [ n ] { ... }` (`ExpectCount`, 0x00015A70).

A keyword table is `{ int id; const char* name; }[]`. The rumble table is at 0x002B2940, six entries from id 0x39, and recognises 145 keywords in `RUMBLE.RMB`.

## Levels.txt (reader: GmLevelManager::Load, 0x0013E1F0)

In `AppInit.JAM`. The game's list of everything it can play, in order:

    Levels [ count ] { Level "NAME" kind [ { NumLoadItems n  Description "text" } ] ... }

* `NAME` is at most eight bytes: the longer one reports "invalid string" and is cut to eight.
* `kind`: `NormalLevel` 0, `HiddenLevel` 1, `NonSelectableLevel` 2, `FMVCinema` 3. `Type`, `ToolCinema`, `DesignerCinema` and `ConversationCinema` are keywords the reader knows and does
  nothing with: the entry's kind stays whatever the heap held.
* Inside a block, any other token is skipped, and an entry with no `NumLoadItems` keeps whatever the heap held. The end of the file inside a block never ends the loop, so a truncated file hangs.
* A second `Levels` block in the file replaces the count and leaks the first tables.

Meaning, from the code that uses the table: a movie entry is played and stepped past when reached (0x0009A820), and only kind 0 goes in the level-select list (0x000A0FC0). `NumLoadItems` is copied
to a field near the loading code (0x00093798), so it is presumably what a loading bar counts to; that is unconfirmed. Hidden and non-selectable are named by their keywords only.

This disc's file has 47 entries: 16 movies, 15 normal, 13 non-selectable, 3 hidden, 17 with descriptions. The list itself is game data and is not in this repository:
`python -I tools/levels.py <image> analysis-local/levels`, then `test_gm_levels analysis-local/levels analysis-local/levels/levels.tsv --print` writes it from recovered code.

## RUMBLE.RMB (reader: GmRumbleEffects, 0x000746A0)

    RumbleEffects [ n ] { RumbleEffect "NAME" { KeyFrames [ k ] { KeyFrame time { Left a Right b } ... } } ... }

Numbers must be reals: `Left 1` is an error. A missing keyword makes the reader return early, leaving the rest to be misread. Effects are found by name through a `GmNameMap`.

## Game text: .AIS (reader: AtTextTable, 0x0001F810)

    NameSpace "NAME"                        optional
    UnicodeChar [ total ] UnicodeString [ count ]        or ASCIIChar and ASCIIString, in either order
    { "NAME" 0x44 0x75 ... 0x00   ... }     wide: numbers up to a zero
    { "NAME" "text"               ... }     narrow: a string, with escapes on

The table holds one block of characters and one of string objects, and is a `GmNameMap` by name. It then registers itself under its namespace, or in the default group with none (`docs/startup.md`, `AtIoContext::AddMap`). A count of strings with no count of characters is reported.
The closing brace is never read. The end of the file before the opening brace is a loop with no end; so is a wide string that runs out of file with a non-zero value left in the tokenizer.

72 distinct files on this disc, 2,685 strings, all read the same by the original and the recovered code.

## Namespaces

`AtIoContext` holds a default `GmNameMapGroup` and a `GmNameSpaceDb`. A namespace is a list of maps, newest first, so a later file's names shadow an earlier one's. An emptied namespace returns to a fixed pool; running out reports `Atlas Base: Name space database overflow.`

## App.ini (reader: GmAppSettings, 0x00098A50)

On the disc at the root, as `{ DataFolder "DataXbox" }`, and read before anything else. The reader knows 46 keywords, for development: `Level` (a starting level, default `ALMOND`), `Resolution`, screen presets including `XB720pScreen` and `XB1080iScreen`, `SkipLegal`, `SkipIntro`, `SkipMainMenu`, `EnableDebugMenu`, `EnableAllUnlockables`, audio switches, and `add_dio`/`no_dio` for debug output channels. Whether a retail build acts on all of them elsewhere is not established.

## Pixel format in text

`AtPixelFormat::Read` (0x00042CB0) reads: bits per pixel as an integer; flags, as an integer or as one or more strings; four masks as integers. **The named form sets the flags inverted:** a flag is ORed in when its name is not the token's, compared without case, so `RGBA` gives every flag but `RGBA`. The numeric form is unaffected. Whether any file uses the named form is not yet checked.
