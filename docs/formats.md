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

## Text formats by extension

Every loader that opens a text file, from the callers of `AtIoTokenFile::Open`, with the number of files on the disc. What a format is for is known only where a reader is recovered or its text was read; other descriptions are absent, not guessed.

| Ext | Files | Loader | Reader state or content |
| --- | --- | --- | --- |
| ASN | 2001 | 0x0003CD60 | Scenes: below. Scene, base nodes and coordinate systems recovered; nine node classes not |
| AGM | 1944 | 0x00038FB0 | Materials: `AtMaterial` and `AtMaterialDatabase` recovered |
| AGT | 1722 | 0x0001C7B0 | Lists of textures: below. The database is recovered; the items are not |
| AKC | 1489 | 0x00026DB0 | Skeletons: below. Recovered |
| ASD | 1380 | 0x0002F7F0 | |
| ASL | 1127 | 0x0003FAC0 | |
| AGD | 1215 | 0x0001ADC0 | Lists of databases (`MaterialDatabase`, `FontDatabase`, `ImageDatabase`, `MatAssignmentDatabase`) |
| AGS | 786 | 0x0002B2E0 | Shaders: below. Recovered |
| ASE | 737 | 0x0003F760 | Emitters: below. Recovered |
| ASB | 632 | 0x00040F90 | Collision shapes: below. Recovered |
| ADR | 311 | 0x00068880 | Design data registries: below. Recovered up to reading values |
| AOB, AOS | 271 each | 0x00025A90, 0x00025640 | Sound banks and segments: below. The lists are recovered; the concrete banks are not |
| AOD | 261 | 0x00018AC0 | Sound databases: below. Recovered up to the concrete banks |
| AGN | 229 | 0x00027D50 | AGN family: below. Recovered |
| GON | 171 | 0x00139090 | Sound notes: below. Recovered |
| AIS | 106 | 0x0001FA70 | Game text: recovered |
| MNG | 104 | 0x00141EA0 | Load lists: base recovered |

Rarer, each with its own loader: AGI 0x00029DC0, AEE 0x000C9A00, AGL 0x0002AC10, ASM 0x0003F390, ASG 0x0003ADC0, GOO 0x0013AA80, AFD 0x00011770, AFE 0x0002CD40, AFL 0x000114A0, AFA 0x000134A0, AFH 0x00012CF0, AFP 0x00012370, AFT 0x000139A0, AFX 0x00020310, GOM 0x0013D2A0, ASC 0x0003FE00, AGA 0x00028340, AGF 0x0002A290, AUD 0x000BC010, ADD 0x0009BD50, AOE 0x00025450, GOD 0x0013AD70, ANA 0x000735F0, ANM 0x00073AD0, AUA 0x000B6140, AUS 0x000BEF80.

Files with no text loader above, by count: DDS 3913, XKA 1489, SDL 1269, SSM 1127, XGG 1092, ANL 1033, VFX 548, WBA 271, TGA 211. Their readers are not yet found.

## Materials (.AGM)

`Material "name" { ... }`, inside a `Material[n] { }` list, often after shader definitions and under `NameSpace "x"`. The keywords are in `AtMaterial::s_keywords`. Colours are four integers, or three for `ConstantColor`, with `ConstantAlpha` the fourth. `SimpleTextureShader n` takes one texture from the store at index n. `Shader "name" firstStage stageCount firstTexture textureCount` names a shader, and `ShaderWithNameSpace` a space too. Blending by `Add` or by `BlendInverseAlpha` also turns fog off. Unless `BlendSeparateAlpha` is given, alpha blends as colour does.

A file is an optional `NameSpace "x"` as its very first token, then in any order `ProgShaderTexture[n] { }`, `StagedShaderTexture[n] { }` and `Material[n] { }`. A list holds names or the word `Default`, meaning none. The `...WithNameSpace` forms take each name followed by a name space; an empty one means no space. `NameSpace` anywhere but first is an error.

## Textures (.AGT)

A file is `Surface2D[n] Texture[n] Texture2D[n] { Texture2D "name" { ... } ... }`, optionally with `Surface3D[n]`, `Texture3D[n]`, `CubeMapTexture[n]`, and `NameSpace "x"` as the first word. On the disc's 579 distinct files: 2,358 textures, 2,424 surfaces, 112 on demand. The body of an item (`PlatformSource`, `DDSSource`, `MipMap n`, `DemandLoad` and so on) is read by the item's own slot 2, which is not recovered. An item whose name the owner already knows is read and made, but not named here.

## Shaders (.AGS)

```
[NameSpace "space"]
Shader[n] {
  Shader "name" {                       eight-byte names
    StagedShaderComponent[n] {
      StagedShaderComponent {
        TextureStage { ...stage... }
        CombineOp Add | BlendAlpha | BlendDestAlpha | Modulate | Modulate2x | ModulateByAlpha | ModulateByAlpha2x | ModulateByAlphaAddColor | Subtract | BlendInverseAlpha
        ModulateOp SelectConstant | SelectDiffuse | SelectTexture | ModulateTextureByConstant | ModulateTextureByDiffuse | ModulateTextureByDiffuseAndConstant
        AlphaSource SelectConstant | SelectDiffuse | SelectTexture | SelectModulatedOutput       default SelectModulatedOutput
        FlipFaceCulling  DisableDepthBuffer  DisableDepthWrites  DisableColorWrites
      }
    }
    ProgShaderComponent[n] {
      ProgShaderComponent {
        CombineOp ...  and the same four switches
        VertexProcess[n] { "name" ... }     at least one; a second list is an error
        PixelProcess "name"
        TextureStage[n] { TextureStage { ...stage... } ... }     at most eight
      }
    }
  }
}
```

A stage: `Texture "name"`; `TextureWithNameSpace "name" "space"`; `CubeNormalizationTexture` (not with a texture); `TxtCoordOffset n`; filter `PointSampled`, `Bilinear`, `Anisotropic x`, `FlatCubic`, `GaussianCubic`; `MipNearest`, `MipLinear` (the default); `Wrap`, `Clamp`, `Border r g b a`; a generator `CoordGenCameraSpaceNormal`, `...NormalWithScaleAndBias`, `...Pos`, `...Reflection`, `...ReflectionWithScaleAndBias`, `...ToonLighting`; `CoordViewToWorldTransform`; `Coord2DTransform n`, `Coord3DTransform n`, `Coord4DTransform n`, cut to a byte; `CoordProjected`. A texture is found in the file's name space, if it has one that is not empty. The disc's 267 distinct files use only staged components.

## Sound banks and segments (.AOB, .AOS)

An `.AOD` file lists them, under `BankDatabase` and `SegmentDatabase`.

* `.AOB`: `Bank[n] { Bank "name" { ... } ... }`. Each entry gives a residence (`DiskResident`, `SoundRamResident`, `PrimaryRamResident`), a format (`MsaFormat`, `MiaFormat`, `RwfFormat`, `WbaFormat`, `SbaFormat`, `GmdFormat`, `GmsFormat`, `PlatformSource`), `BlockID n`, `DelayLoad`, `Streaming`. Real files have only `PlatformSource`, with `Streamed`, `StreamedStereo` or `StreamedMemory` after it in 78 of 161 uses; those three are taken and ignored.
* `.AOS`: `Segment[n] { Segment "name" { Bank "bank" "namespace" index  Looping } ... }`. A segment names an index in a bank.

Both readers stop after the first `Bank` or `Segment` block. Not established: what a bank's data file is, what the format values mean to the player, what `BlockID` selects.

## Scenes (.ASN)

```
NameSpace "x"                 optional
RotTransCoordSys[n]           any order, each optional: counts only
ScaleCoordSys[n]
MotionState[n]
Node[n]                       the size of the name map. Required, and ends this part
ClassWord[count]              then any number of lists, one to a class
{
    ClassWord "name" RotTransCoordSys MotionState ScaleCoordSys
    {
        Position, Quaternion, BoundingSphere, Attach, ... the words of that class
    }
}
```

`Node[n]` has two meanings: first the total of names, later a list of plain nodes. Real files have both. The 652 distinct files use five classes: Node, ModifierNode, ModelActor, JointedModelActor, ParticleSystemActor. Keyword table 0x002B1990, 116 words.

The base node reads BoundingSphere, ActiveDistance (stored squared), Attach, AttachBase, AttachNodeToThisJoint, AttachNodeToThisBase, Position, OrthoNormalize, Quaternion, LookAt, LinearVelocity, AngularVelocity, CollisionEnable and Disable, BoundingGeometry, LineSegIntersectEnable and Disable, two MatAssignment words whose integer is thrown away, and four debug draw words that do nothing. The modifier node adds Modifier, ModificationEnable and Disable. Mesh, animation channels, emitters and the rest belong to classes not recovered.

## AGN family (.AGN, .AGA, .AGI, .AGF, .AGL, .ASL)

| Ext | Loader | Holds | Real files |
| --- | --- | --- | --- |
| .AGN | 0x00027D50 | Named lists of materials, `MatAssignment` / `MatAssignmentWithNameSpace` | 61 |
| .AGA | 0x00028340 | Material animations: `FrameNumber`, `MaterialKey`, `MatAnimation` with `Keyframes`, `Rate`, `Length`, `LoopPeriod` | 7 |
| .AGI | 0x00029DC0 | Images: `MatAssignment`, `Image` | 102 |
| .AGF | 0x0002A290 | Fonts: `Font[n]`, `MatAssignment`, `Font2D`; `Font3D` stops with a message | 2 |
| .AGL | 0x0002AC10 | Lights: colours, `Range`, `Attenuation`, cone angles | 17 |
| .ASL | 0x0003FAC0 | Lens flares, by its keyword table. **No file on the disc is this.** | 0 of that format |

The three disc files named .ASL are in `DESIGN\BEHAVIOR` and begin with binary and names such as `func0000`: compiled scripts of some kind, not recovered here and not established further. Run through 0x0003FAC0 they are read as bad text, and the oracle compares that, which shows little. So the lens flare's fields are tested by written text only.

## Skeletons (.AKC)

The executable never says. It has the three letters, as an extension, at 0x00273128, and no message that names the format. What the code and files show: an .AKC file is one `AtCoordSysDatabase`, read at 0x00026DB0 and called at 0x0001A692 for each name in a list the owner keeps (0x002C62AC). It holds a tree of named coordinate systems, and for some of them a matrix given in full.

```
RotTransCoordSys[n]            at least one of these two counts
ScaleCoordSys[n]
LocalDrawingTransform[n]       optional: how many entries give the word below
CoordSys
{
    RotTransCoordSys "rootR"   Position x y z, Orientation x y z w
    {
        Parent "name"          must have come earlier in the file
        LocalDrawingTransform  sixteen reals
    }
    ScaleCoordSys "name"       Scale x y z, or UniformScale s
    {
    }
}
```

Words are the original's, table 0x002B05F8. On the disc's 544 distinct files: 2,130 rotations, 65 scales, 1,220 drawing transforms; `UniformScale` is on no file. 474 files have no drawing transforms, most with one to three entries (for example `UI\TRANS\WIPE.AKC`: `bone0R`, `bone1R`); the 70 that do are mainly under `ART\CAST` with names such as `rootR`, `pelvisR`, `rtthighR`. "Skeleton" is my word for this, from those names. 0x00032940 multiplies `m_drawingLocal` by the world matrix into `m_drawingWorld`, and the values on real files look like the inverse of the pose at rest, which is what skinning wants: an inference from the numbers, not recovered code.

Each entry takes two matrices from the pool as it is made, and each drawing transform two more: hence `(rotTrans + scaled + extra) * 2` in `Allocate`.

## Sound databases (.AOD, .AOE)

Established from the code, its keyword tables, its messages and the text:

* An `.AOD` file names other files, relative to its own directory: `BankDatabase[n] { "name" ... }` (`.AOB`), `SegmentDatabase[n]` (`.AOS`), `EmitterDatabase[n]` (`.AOE`), `DatabaseLoader[n]` (another `.AOD`). No file on the disc uses `DatabaseLoader`. Only `AUDIO\GLOBAL.AOD` uses `EmitterDatabase`.
* An `.AOE` file is a list of emitters: `Emitter[n] { Emitter "name" { ... } ... }`, with the words `Center`, `Stereo`, `Emitter3D`, `Position x y z`, `Orientation x y z w`, and `NameSpace` before the block. An emitter is a named place a sound comes from: an `AtCoordSys` with a kind and perhaps an SDK stream. `Emitter3D` makes a stream of kind 2. `Center` makes a stream of kind 1 on mix bin 2, which is the front centre. `Stereo` and nothing make no stream. There is one real file, `AUDIO\EMITTERS.AOE`, with 50 emitters: `sfx`, `ui`, `voice`, `music`, `ambience`, `collsn1-5`, `actor1-40`.
* Position and orientation are real numbers. An integer there is reported as "expected real".

A finding the earlier notes did not have: 38 list files lie **loose on the disc**, under `/DataXbox/Audio/Music/`, in no archive, beside their `.wba` files: 8 `.aod`, 15 `.aob`, 15 `.AOS`. The text cases come only from the archives, so they lack these, and `AUDIO\GLOBAL.AOD` names four of them. `tests/oracle/aod_loose_cases.py` writes them to `analysis-local/scratch/aod/loose`, and the oracle will not run without them. To regenerate: `python -I tests/oracle/aod_loose_cases.py "<disc image>" analysis-local/scratch/aod/loose`, from the main checkout. The integrator may prefer this in `tools/jam.py textcases`; I was not to edit that. The `aob` suite has not seen these 30 lists either.

```
.ANM
[NameSpace "space"]
AbstractMapper[n]
{
    AbstractMapper "name"
    {
        ButtonMap[n] { ButtonMap { PhysicalID code  AbstractID code  [PhysicalButtonState word] [AbstractAxisState word] [AbstractPOVState word] } ... }
        AxisMap[n]   { AxisMap   { PhysicalID code  AbstractID code  [PhysicalPOVAxis AxisX|AxisY] [AbstractPOVAxis AxisX|AxisY] } ... }
    }
    or
    AbstractMapper "name" { AbstractMapper "earlier" "space or empty" }
}
Codes: Key* 0x1000xxxx, Mouse* 0x2000xxxx, JoystickButton0-63 0x3000xxxx, Axis* 0x4000xxxx, POV 0x50000000; table at 0x002B2C10, 235 entries, two pairs sharing a value.

.AFX
NameSpace "space"
TextFormat[ n ]
{
    TextFormat index
    {
        BoundsRectangle r r r r | WordWrap | MultiLine | Password | ReadOnly | NoSelect | Border | FontHeight r | TextColor i i i i | TextMaxLength i
        Alignment AlignmentLeft|Right|Center|Justify | LeftMargin r | RightMargin r | Indent r | Leading r
    }
}

.AFL
Level { HeapSize i  MaxVariableValueLength i  MaxVariables i  FontSourceLevel i|Self|All }
```

## Modifiers and space graphs (.ASM, .ASG)

Both are established from the keyword tables at 0x002B1D40 (58 words) and 0x002B1800 (19 words) and from what the readers do with them, not from the extensions.

**.ASM: modifiers.** Named objects, one name map to a file; the map goes into the owner's `m_modifiers` (+0xCC) or, under a name space, `m_modifierSpaces` (+0xE4). That is what fills them. Scene `ModifierNode`s and graphs look names up there.

```
NameSpace "x"                  optional
Modifier[n]                    the word is not checked; n is the size of the map, and at least 1
LightModifier[n]               then any lists, any order
{
    LightModifier "name" { Light "name" "ns"  Directional | Point | SpotLight  Position x y z  Direction x y z  LightAnimation "name" "ns"  Priority n }
}
AmbientModifier[n] { AmbientModifier "name" { Color r g b a  Priority n } }
PerspectiveModifier[n] { PerspectiveModifier "name" { FieldOfViewY v  Priority n } }
ContextConstantModifier[n] { ContextConstantModifier "name" { VertexProcessConstant[m] { a b c d i ... }  PixelProcessConstant[m] { a b c d i ... }  Priority n } }
GroupModifier[n] { GroupModifier "name" { Modifier[m] { "name" "ns" ... }  Priority n } }
BlockModifier[n] { BlockModifier "name" { Block <class word>  Priority n } }
```

The table holds many other words (TextureProjectorModifier, ProjectedShadowModifier, CubeMapModifier, MaterialOverrideModifier ...). No reader here takes them: they report a keyword error. The 17 real files use only LightModifier (Point or Directional), AmbientModifier, GroupModifier.

**.ASG: a space graph**, a binary partition of space. See the head of `include/at/AtSpaceGraph.h` for the grammar. Real files, all 16: BinarySpace and LeafSpace only, Partition, PosChild, NegChild, BoundingBox, Draw, Cell, Modifier. No X/Y/ZBinarySpace, no LineSegIntersect, no NodeLink beyond a count.

`AtSceneModifierSet` of the scene work **is this class**: renamed `AtSpaceGraph`. Its "links" are the file's NodeLink, their "items" NodeIncidence. `AtSceneDirtyOwner` was a leaf.

What a graph does on load: reads counts; makes pools and arrays; reads each space. A leaf attaches its Draw and LineSegIntersect nodes at once; other spaces keep them in a pending list, and after everything is read pass them to all leaves under them. Then it walks the tree with a stack of modifier states (the owner's +0x210, sized by its +0x264 and +0x268, made and freed for the walk). A space with a modifier pushes a copy and applies it; each leaf keeps a copy of the top. So each leaf ends with, per kind of modifier, what is in force there: lights accumulate in priority order, up to 7; others replace, unless what holds the slot has a higher priority; a block empties a kind's slot; a group applies its members.

## Lists of databases (.AGD)

The executable never says. The three letters are an extension at 0x002723CC, and the words are at 0x002B0190. From the code and all 423 distinct files: an .AGD is a list of other files to load, by kind, with nothing else in it.

```
TextureDatabase[1]
{
    "Name"
}
MaterialDatabase[2]
{
    "A\B"
    "C"
}
```

Eighteen words: `TextureDatabase`, `ShaderDatabase`, `MaterialDatabase`, `MeshDatabase`, `LightDatabase`, `CoordSysDatabase`, `CoordSysMirrorDatabase`, `WeightsDatabase`, `AnimationDatabase`, `MatAssignmentDatabase`, `MatAnimationDatabase`, `MorphAnimationDatabase`, `LightAnimationDatabase`, `FontDatabase`, `ImageDatabase`, `VertexProcessDatabase`, `PixelProcessDatabase`, `DatabaseLoader`.

Uses on the disc's distinct files (lists, then names):

| Word | Lists | Names |
| --- | --- | --- |
| MaterialDatabase | 420 | 688 |
| TextureDatabase | 414 | 652 |
| MeshDatabase | 352 | 461 |
| CoordSysDatabase | 289 | 699 |
| AnimationDatabase | 289 | 699 |
| ShaderDatabase | 228 | 298 |
| MatAssignmentDatabase | 62 | 67 |
| LightDatabase | 16 | 16 |
| MatAnimationDatabase | 7 | 7 |
| FontDatabase | 1 | 1 |
| ImageDatabase | 1 | 1 |

The other seven words are on no file. Three comments occur.

Order in the file does not matter to loading: 0x0001A4B0 goes vertex processes, pixel processes, textures, shaders, materials, meshes, lights, coordinate systems, mirrors, weights, animations, material lists, fonts, images, material animations, morph animations, light animations; frees the names; then nested loaders. Names are relative to the .AGD's own directory, which is pushed while they load.

## Emitters (.ASE)

Text, in the engine's own tokens: `Emitter[n] { Emitter "name" { word values ... } ... }`, with an optional `NameSpace "x"` first. It has nothing to do with 3D Studio's ASE, despite the extension. 221 files on the disc hold 364 emitters between them. The 47 words are at 0x002B1F20; `ParticleRotationSpeed` sits out of the order of its id there, and the table keeps it so.

Words the real files use: DeltaInitialSpeed, DiffuseColor, EmissionCone, EmissionConeInnerAngle, EmissionConeScale, EmissionEventChance, EmissionLine, EmissionPoint, EmissionRectangle, EmitterLifetime, LinearColorChange, LinearColorChangeKeys, LinearMotionBlurKeys, LinearRectChange, LinearSquareSizeKeys, LineSegIntersectTesting, LocalAcceleration, LocalBoundingSphere, MatAssignment, MaxEmissionEventRate, MaxParticles, MaxParticlesPerEvent, MotionBlur, ParticleChance, ParticleInitialSpeed, ParticleLifetime, ParticleRotationSpeed, QuadraticColorChangeKeys, QuadraticMotionBlurKeys, QuadraticRectSizeKeys, QuadraticSquareSizeKeys, RectWidthHeight, SquareSize. The rest are reached only by written text.

## Collision shapes (.ASB)

Text, a sibling of `.ASN` in form but with a table and a reader of its own: named collision shapes. This comes from the reader at 0x00040AA0, the keyword table at 0x002B21A0 (14 words), the original's message "Node type must be jointed for bounding geometry", and the 239 distinct real files.

```
NameSpace "x"                  optional
BoxBoundingGeometry[n]         any order, each optional, counts only
SphereBoundingGeometry[n]
GroupBoundingGeometry[n]
BoundingGeometry[count]        the list proper. End of file before it is accepted: empty
{
    BoxBoundingGeometry "name"    or Sphere..., Group...
    {
        Parent "name"             earlier in the file
        RotTransCoordSys 1        a joint of the node's skeleton, from 1; none or 0 is the node's own coordinate system
        Box cx cy cz hx hy hz     half sizes; absolute values are taken
        BoxMinMax x y z x y z     or corners
        Sphere cx cy cz r         on a sphere; absolute radius
    }
}
```

ConeBoundingGeometry, CylinderBoundingGeometry, Cone and Cylinder are in the table, and nothing reads them: they are reported. No real file uses NameSpace, BoxMinMax, Cone or Cylinder. 172 of the 180 `RotTransCoordSys` in real files say 1; the highest is 4. Real files hold 271 boxes, 40 spheres and 240 groups, and the groups are the roots the rest hang from.

```
.ANA
[NameSpace "space"]
AbstractController[n]
{
    AbstractController "name"
    {
        Buttons i | Axes i | POVs i | EventHandlers i | DeadZone r | Saturation r
    }
}
Each word optional, any order; a saturation of 100 and everything else 0 by default. Keyword table 0x002B2828. Inputs get ids 0x30000000, 0x40000000, 0x50000000 up.
```

## Design data registries (.ADR)

Text, through the ordinary tokenizer. 103 distinct files in the jams, under `DESIGN\REGISTRY\` and `DESIGN\LOCATION\...`. The words come from the table at 0x002B2690.

```
[NameSpace "Name"]
DataComponentTypes[n] { DataComponent { Type T NumComponents c } ... }
DataRegistry[n] { DataLink { Name "N" Type T Parent "A\B\" NumElements e Floor f Ceiling c FormatString "F" Data ... WillAcceptParentData } ... }
```

Twelve types: Empty, Boolean, Int, Real, ShortName, String, and arrays of Boolean, Byte, Int, Real, ShortName, String. The real files use all but ByteArray; none uses FormatString or a Unicode string. The first list sizes a pool per type; the second fills a tree of named values, which can hang in a parent registry's tree. It is the game's tunable design data: `Gravity`, `DamageParticles`, actor prototypes inheriting by path.

## Library and record databases (.ADD, .ADB)

* `.ADD` is text through the shared tokenizer, keyword table 0x002B47F0 with two words: `DataRegistry` (0x39) and `DatabaseManager` (0x3A). Each is followed by `[ n ] { "name" ... }`. Anything else is reported (code 0x1E) and reading goes on. A second list of the same kind is reported (code 0x27) and replaces the first, leaking it.
* Each `DataRegistry` name is loaded as an `.ADR` through the registry at 0x000674E0 / 0x00068880; each `DatabaseManager` name as an `.ADB` through 0x000B2330. All names are read first, the file is closed, and the entries are loaded with the `.ADD`'s directory pushed on the path.
* Start-up, 0x0008DEA0, loads `Design\Registry\Generic.ADR` into App+0x6950, then `Library.ADD` into the library at App+0x6B40, as `Load(owner, "Library", 0, parent)` where owner is offset 0x394 of the object at 0x003B357C and parent is offset 0x2C of that object. 0x0008ED60 calls the library's Clear.
* **The disc has exactly one `.ADD`, `AppInit.JAM`'s `LIBRARY.ADD`, holding only a `DataRegistry` list of 11 names (`Design\Registry\Actor` ... `Weapon`), and no `.ADB` in any archive** (a scan of every archive's directory: 1 ADD, 0 ADB, 311 ADR). So on this disc the whole `.ADB` half is never reached; it is recovered because it is what the reader alone needs, and it is tested on written text only.
* `.ADB`, keyword table 0x002B5310 (34 words, names in the binary): an optional `NameSpace "x"`, then `DataComponentDescs[n] { DataComponentDesc { Name Type NumElements Floor Ceiling FormatString } ... }`, `RecordLayoutDatabases[n] { RecordLayoutDatabase { Name "x" Desc "..." DataComponents[n] { "field" ... } } ... }`, `DataRecordDatabases[n] { DataRecordDatabase { Name Desc RecordLayout "layout" DataRecords[n] { values ... } } ... }`. Values are read by the value classes of the `.ADR` work, by type: Boolean 1, Int 2, Real 3, ShortName 4, String 5, BooleanArray 6, ByteArray 7, IntArray 8, RealArray 9, ShortNameArray 10, StringArray 11; `Relational` gives 12, which no table holds. `Byte`, `Flags`, `Database`, `RelatedField`, `ReferencedField` are in the table but rejected where they could appear.

## Sound notes (.GON)

Not a game object file. A `.GON` file is the table that says which sound an animation's trigger plays: "sound notes". Established from the reader 0x00139090, its keyword table at 0x002B9090 (12 words), and the 49 distinct real files, all under `AUDIO\` (`AUDIO\CAST\ENEMIES\*`, `AUDIO\CAST\PCS\*`, `AUDIO\LOCATION\*`, `AUDIO\SFX\*`, `AUDIO\OBJECTS\*`), one to a character, location or family of effects.

```
NameSpace "x"                optional; the bank's name, which a search can filter on
SoundNotes[n]
{
    Note "name"
    {
        AnimationTrigger "t"     the name an animation fires; the note is found by it
        VFXTrigger "v"           entered in a third map, which nothing recovered reads
        GameEvent "e" "arg"      the second string optional; used by 0x00160030 through 0x001511E0
        ActorEmitter             the default: does nothing
        GlobalEmitter "sfx"      an emitter by name, from the sound owner's table (EMITTERS.AOE)
        Conditional "voice"      an emitter: 0x001386D0 plays nothing while it has a voice playing
        Stop "other"             0x001386D0 first stops the first segment of that note
        Singleton                0x001386D0 skips the play if the segment already plays on the emitter
        Segments[n] { "seg" 0.25 "seg2" ... }   names in the .AOS lists, each with an optional real weight, default 1; the load scales them to sum to 1
    }
}
```

Real files: 517 notes, 517 AnimationTrigger, 513 Segments blocks, 510 ActorEmitter, 7 GlobalEmitter, 4 Conditional, 4 Stop, 14 Singleton; no NameSpace, VFXTrigger or GameEvent, and no weights. Words are case-insensitive. Anything else inside a note, word or token, goes by without a report.

What the notes are for, from their callers (not recovered): `GmAcAnimationCallback` (RTTI name, vftable 0x0027C1D8) slot 4, 0x00160030, takes an animation trigger, finds the note by `FindNoteByTrigger` (first in the actor's own bank name, then any) and plays it with 0x001386D0; 0x00166FC0 does the same from a table of event handlers; 0x0017E760 finds notes by name for another class.

## Level manifests (.MNG)

A `.MNG` file is a level's load list. 51 distinct real files: `LOADER.MNG` under each `DESIGN\LOCATION\<level>\` (31), one to a playable character and look under `ART\CAST\PCS\...` (18), `PERSIST.MNG` in `Persist.JAM` and `UI\UI.MNG` in `MainMenu.JAM`. Each word names a kind of database and is followed by a counted list of paths:

```
// Level:    Lbkyrd1
SoundDatabaseLoaders[ 16 ]
{
   "Audio\Objects\bzappr\bzappr"
   ...
}
SceneDatabasesLoaders[ 60 ]
{
   "Art\Location\Lbkyrd\vistest" InSpaceGraph InsertInSnoopie
   "Art\Objects\Region\BoxReg" NotInSpaceGraph
   ...
}
VFXDatabases[ 22 ] { "Art\VFX\ItmspF\ItmspF.VFX" ... }
```

Two keyword tables, the original's words: the base's ten at 0x002B9350 and the level's sixteen at 0x002B9200 (ids 0x39 to 0x52). The level's Parse makes a tokenizer of its own, has the base install its words on it, and then installs its sixteen inheriting them.

| Word | Id | Who | Item, extension, size | Real files: paths |
| --- | --- | --- | --- | --- |
| EventDatabases | 0x39 | base | (0x1B4880), 0xB8 | 59 |
| InstanceRegistries | 0x3A | base | (0x1B4A10), 0x288 | 233 |
| SoundDatabaseLoaders | 0x3B | base | AOD (0x1B3FF0), 0x9C; each runs as soon as it is configured | 285 |
| GraphicsDatabasesLoaders | 0x3C | base | (0x1B4220), 0x98 | 1158 |
| BoundingGeometryDatabase | 0x3D | base | (0x1B46F0), 0xBC | 0 |
| SceneDatabasesLoaders | 0x3E | base | (0x1B43A0), 0x10C; reads InSpaceGraph, NotInSpaceGraph, InsertInSnoopie after each path | 1332 |
| CinemaDatabaseLoaders | 0x3F | base | (0x1B4560), 0xB8 | 0 |
| BehaviorAnimationLists | 0x43 | level | ANL, 0x94 | 0 |
| BehaviorSoundLists | 0x44 | level | SDL, 0x94 | 0 |
| SmallCScripts | 0x45 | level | ASL, 0x94 | 0 |
| TriggerDatabases | 0x46 | level | AET, 0xB0 | 42 |
| SubscriberDatabases | 0x47 | level | AES, 0xB0 | 72 |
| VFXDatabases | 0x48 | level | VFX, 0x94; the path carries its own `.VFX` | 551 |
| DynamicNodes | 0x49 | level | DAC, 0x198, an AtScene in place | 0 |
| Videos, Streaming, Texture, Node | 0x4A-0x4D | level's table | none: they go to the base, which lets them by | 0 |
| SoundNodeDatabases | 0x4E | level | GOO, 0x94 | 19 |
| SoundNoteDatabases | 0x4F | level | GON, 0x90 (other branch) | 171 |
| MusicChainDatabases | 0x50 | level | GOM, 0x90 | 32 |
| CollisionSoundNodeDatabases | 0x51 | level | GOC, 0x94 | 0 |
| InstanceSoundNodeDatabases | 0x52 | level | GOI, 0x94 | 0 |

Every word reads `[ n ] {`, allocates n items with new[] from the heap at 0x003B3610 (pushed around the allocation; null is reported with -2, no memory with -4), configures each from a request on the stack (`m_path` at +4, a context at +0x84 and +0x88), then reads `}`. After the parse, AfterParse queues the base's arrays and then the level's, in a fixed order, and the time-sliced loader runs them one at a time; Finished registers the sound nodes', the sound notes' and the instance sounds' data once all have run.

## Scene lists and cell graphs (.ASD, .ASC)

Established from the keyword tables at 0x002B13A0 (11 words), 0x002B2120 (12), 0x002B18A8 (11) and 0x002B1910 (14), from what the readers do with them, and from the real files; not from the extensions.

**.ASD: a scene's list of files.** It is to a scene what .AGD is to graphics, and its own word for its kind is the same, `DatabaseLoader`. It holds no data of its own: every word is a list of leaf names, `NodeDatabase[n] { "name" ... }`, and the loader opens each in the directory of the .ASD. So yes, it ties the other scene formats together: for one object or location it names the .ASN nodes, .ASM modifiers, .ASG space graph, .ASB bounding geometry, .ASE emitters, .ASL lens flares, .ASC cell graphs, .AIS strings, .ASI flat images, .AST flat text, and further .ASD lists. Whatever order the file gives, they load in a fixed one: strings, text, images, geometry, modifiers, emitters, lens flares, nodes, cell graphs, space graphs, then nested lists. That order matters: a space graph names modifiers and nodes, which are by then loaded. The owner is the scene owner (`AtNameTableOwner`); the game makes the list at 0x000A1608 and loads "UI\Trans\wipe" with it, and 0x001B4384 loads others.

The 409 real files use six words: NodeDatabase 409, BoundingGeometryDatabase 211, EmitterDatabase 130, SpaceGraph 13, ModifierDatabase 13, CellGraphDatabase 9. Never DatabaseLoader, LensFlareDatabase, StringDatabase, ImageDrawDatabase or TextDrawDatabase. So .ASI and .AST are reached on the disc by no list, and there are none among the text files.

**.ASC: cell graphs**, rooms (cells) and the portals between them, for visibility. See the head of `include/at/AtCellGraph.h` for the grammar. A portal is a polygon in a mesh's vertex array, 3 to 31 vertices; it keeps a pointer into the locked array, the centre of its vertices and an unnormalised normal from the first three; a cell is a run of portals. All 9 real files have one graph each, a Mesh, OppositeCell (never OppositeCellDifferentGraph), Vertices, ClosedByViewerDist, Cell, Portals and ExteriorPortals. What a space graph leaf's `Cell` word finds is a cell of one of these: `AtSpaceCellTable` of the .ASG work **is `AtCellGraph`**, now a typedef.

**.ASI and .AST: flat images and text**, things drawn on the screen, each item a block of words with a position, scale, clip rectangle, four corner colours and an image or a font and string. See `include/at/AtDrawItem.h`.

## Flash UI levels (.AFD, .AFP, .AFH, .XFF)

From the code and the 20 real files, one per user interface archive (`DataXbox/UI/Xbox/*.JAM`):

```
.AFD    Loader { Level "name"  Sprite "name"  Shape "name"  Text "name"  Script "name"  [Sound "name"] }
        Each word loads at once, in the file's order: Level is name.AFL; Sprite is Sprites\name.AFP and the frames; Shape is Shapes\name.AFH and the .AGD it names; Text and Script are not recovered.
        Sound is read and dropped. Any other word is reported and the loop goes on.
.AFP    NameSpace "s"  Sprites[n] { SpriteDef { index FrameRefs[n] { frame,"name" | frame; | from:to | frame*times ... } } ... }
.AFH    NameSpace "s"  GraphicsDatabaseLoader "agd"  Shape[ n ] { Shape index { Rectangle r r r r  MeshAssignment i "name" } ... }
.XFF    binary, little-endian. 16 bytes: "HVSIXFF\0", then 8 bytes not read here (01 00 01 00, and the file's size).
        16 bytes: u16 frames, u16 offset of the data, u32 size of the data, u16 places, u16 actions, u16 removes, u16 sounds.
        The data, at that offset: per frame, u32 index, u8 places, u8 actions, u8 removes, u8 sounds; then that frame's tags, removes first, then places, actions, sounds.
        An action, remove or sound is a u32. A place is 0x18 bytes: u8, u8 (not read), u8, u8, u16, u16, u32, then three u32 offsets, counted from the end of the record, of a name, a 6-word transform and an 8-byte color
        (4 bytes, then 4 scaled by 1/255); 0 means none. Every tag of a kind comes from one run of objects for the whole file, in file order.
.AFF    the text form of .XFF, "Frames[n] { FrameDef ... }": read when the environment's +0x1F4 is 0. No .AFF is on the disc, and its frame reader is not recovered.
```

Which of .XFF and .AFF is read is decided by the .AFE environment (+0x1F4): the 20 .AFE files all have the word `PlatformSpecificFiles`, and all 20 archives hold an .XFF and no .AFF. The name of that field is a guess from the word.

## Binary meshes and animations (.XGG, .XKA)

From the readers, and checked on every real file by the oracle. Both start with 0x10 bytes that no reader looks at: on the disc, `HVSI`, the extension and a terminator, then two 16-bit numbers and a word (an `.XKA`: 2, 2; an `.XGG`: 2, 13). Then a table, then blocks, each read whole; the reader aligns to 4 after the table. Every pointer the readers make is a block's address plus a 32-bit offset from the table. Layouts: `AtXkaHeader`, `AtXkaAnimation` and `AtXggHeader` in `include/at/AtGrD3DDatabase.h`.

* `.XKA`: a table of 0x78 bytes; a scratch block (names of eight bytes, records of 0x60, read onto the token file's heap and freed) and a kept block (component lists and keys, owned by the database). Records are copied word for word into `AtAnimation`, their 32-bit runs cut to 16 bits. The keys get counts and pointers into the kept block, and flag 0x400, so that clearing does not free what they did not allocate. On the disc: 565 distinct files, up to 116 animations each, none empty.
* `.XGG`: a table of 0x58 bytes; a scratch block of records, a kept block on the mesh heap (aligned to 32 bytes at least while it is allocated), and a third block in memory for the graphics processor, absent in 42 of 442 files. Then items, kind by kind: vertex arrays (records of 0x3C), index arrays (8), streams (0x48), meshes (0x18, a name at 0x0C, with triangles, parts and material references beside). Each item reads its own part and moves the cursors on.

## Flash texts and scripts (.AFT, .AFA)

From the code and the 20 real files, one of each per user interface archive (`DataXbox/UI/Xbox/*.JAM`):

```
.AFT    Text\name.AFT: NameSpace "s"  Text[ n ] { Text i { String "8 bytes"  Font2D "8 bytes"  TextFormat i  Variable "string" } ... }
        The words inside a Text in any order, each optional; any other word is reported (code 0x27) and reading goes on.
        Loaded with, in this order: Text\name.AFX (the formats, already recovered), Text\Font.AGF when the file system has it (2 of the 20 archives), Text\name.AFT, then Text\name.AIS (the strings).
.AFA    AScript\name.AFA: NameSpace "s"  [ActionScriptFileVersion r]  ScriptBlocks [ n ] { Script { ScriptID i  ScriptBlockSize i  Instructions[ k ] { len b1 .. blen  ... } } ... }
        Only NameSpace is checked. After it, the reader takes tokens by position: one more if the first is ActionScriptFileVersion, then the count is the third token's number, and so on.
        Each instruction is a length and that many bytes (each token's low byte), packed one after another into a block of ScriptBlockSize. The bytes are Flash action codes as 16-bit fields (150 = Push, 29 =
        SetVariable, 154 = GetURL2, 157 = If, 153 = Jump, 140 = GotoLabel, 0 = End, by the SWF numbering).
        The strings are AScript\name.AIS, loaded first, through AtIoContext::s_instance (0x002C62F0), not the file context.
```
