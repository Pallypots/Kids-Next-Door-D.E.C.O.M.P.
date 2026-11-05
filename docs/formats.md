# Data formats (reference build knd-xbox-usa-5454008a-v1)

Observations from file magics and the XBE's strings. **No format below has been parsed or recovered yet.** Nothing
here is copied from game data beyond identifying magic bytes. The disc listing is regenerated locally with
`python -I tools/xdvdfs.py ls <image>`.

| Type | Count / total bytes | First bytes | Observation / hypothesis |
| --- | --- | --- | --- |
| `.JAM` (`DataXbox/Jamfiles/Xbox/{,Arenas,Levels,Viewers}`, `UI/…`) | 56 / 958,661,566 | `FSTA` + u32 + u16 + … + `none` | Main asset archives. The XBE builds paths with `JamFiles\XBox\%s%s.JAM`. `AppInit.JAM` is 623 MB. |
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

Next format work (after more engine code is recovered): the `FSTA` JAM container, starting from the code that
formats `JamFiles\XBox\%s%s.JAM` and reads the file, never from guessing at the bytes alone.
