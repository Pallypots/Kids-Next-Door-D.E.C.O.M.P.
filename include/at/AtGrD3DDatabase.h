// The Xbox graphics layer's databases: the concrete class of AtDatabaseLoader that a real build makes, and the two of its kinds whose files on this disc are binary, meshes (.XGG) and animations (.XKA). Also the two
// platform factory functions that make a loader and a sound database. All class and member names are inferred. "AtGrD3D" follows the original's own message at 0x0028B0BC, "AtGrD3DTm2Loader::vRead", which shows the
// graphics layer's classes were named so. See docs/worklog/2026-11-07-loader.md.
//
// What a binary form is, from the readers: a file of 0x10 bytes of header that nothing reads, a table of offsets and counts, and blocks. The readers load the blocks whole and make pointers from offsets into them. Nothing
// in them is a host pointer: every pointer made here is a block's address plus a 32-bit offset from the file.
//
// The seams. What is not recovered is behind src/at/AtGrD3DUnrecovered.cpp, whose bodies all go to a handful of functions in src/at/AtGrD3DSeam.cpp. Those stop with a diagnostic; a test supplies them instead:
//   * The items a mesh database makes (meshes, vertex arrays, index arrays, vertex streams): constructors, destructors, and their binary readers. They hold Direct3D resources.
//   * The eight other kinds of database the loader makes (textures, materials, shaders, lights, fonts, images, vertex and pixel processes): the classes exist, with their sizes and the slots their bases require, and every
//     body stops, the constructor first. Their items are textures, surfaces and the like.
//   * Memory for the graphics processor (0x00205EA0, 0x00205EE0), which a mesh's third block lives in.
//   * The keys' own slots other than 1, 3, 6 and 14 to 18: allocating, interpolating, and reading text keys. So animations load and cannot yet be sampled.
#pragma once

#include "at/AtAudioBank.h"
#include "at/AtDatabaseLoader.h"

class AtSoXboxDatabase;
class AtGrD3DMeshDatabase;
class AtGrD3DTextureDatabase;

// ---- The seam functions: src/at/AtGrD3DSeam.cpp, or a test's ----------------------------------------------------------------------------------------------------------------------------------------------------------

unsigned int AtGrD3DUnrecovered(unsigned int va, void* object, unsigned int a, unsigned int b);  // stops, naming the original's function
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int alignment);  // 0x00205EA0 memory the graphics processor can read; the alignment below a page goes as 4 KB
void AtGrD3DFreeContiguous(void* data);                                     // 0x00205EE0
class AtGrD3DVertexArray;
class AtGrD3DIndexArray;
class AtGrD3DVertexStream;
class AtGrD3DMesh;
struct AtGrD3DOwner;
// The items' binary readers. Each moves the cursors past what it reads.
void AtGrD3DReadVertexArray(AtGrD3DVertexArray* array, char** records, char** kept, char** contiguous);  // 0x000EC8C0
void AtGrD3DReadIndexArray(AtGrD3DIndexArray* array, char** records, char** kept);                       // 0x000F7EE0
void AtGrD3DReadVertexStream(AtGrD3DVertexStream* stream, AtMaterialOwner* owner, char** records, char** kept);  // 0x000EE750
void AtGrD3DReadMesh(AtGrD3DMesh* mesh, AtMaterialOwner* owner, AtGrD3DMeshDatabase* database, char** triangles, char** parts, char** materials, char** records, char** kept, char** contiguous, char* name);  // 0x00122550

// ---- The binary file ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// A file opened for reading in blocks. The same shape as AtIoFileRef, but its methods are a second set, which look for files through a context of their own and do not test m_fs when closing: see AtAudioBankFile, which
// shares its constructor and destructor and is perhaps the same class.
class AtIoBinaryFile : public AtAudioBankFile {
public:
    int Open(const char* path, int flags);                                   // 0x000249D0 1 when no file system holds the file
    int OpenWithExtension(const char* path, const char* ext, int flags);     // 0x00024AA0 ext is added if the leaf has no dot
    int ReadAll(void* out, unsigned int size, unsigned int* done);           // 0x00024A50 waits for a pending read
    int Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request);  // 0x00024950
    int Release(AtIoFileRequest* request);                                   // 0x00024980 0 when there is no file
    void Close();                                                            // 0x000249A0 the same code as AtAudioBankFile's destructor, folded; called by name before it

    static AtIoContext* s_context;  // 0x002C6328
};

// Reads a whole block, waiting for it if it is pending. The original writes this out at each read, the same each time.
int AtReadBlock(AtIoBinaryFile* file, void* out, unsigned int size);

// ---- The owner as the graphics layer sees it ----------------------------------------------------------------------------------------------------------------------------------------------------------------------

// The object every database is loaded for, past what AtMaterialOwner knows of it: lists of the databases the graphics layer keeps. Probably the graphics device; its vftable and constructor are not recovered.
struct AtGrD3DOwner : public AtMaterialOwner {
    void AddMeshDatabase(AtGrD3DMeshDatabase* database);     // 0x000EF140 at the head
    void RemoveMeshDatabase(AtGrD3DMeshDatabase* database);  // 0x000EF160 if it is there

    char m_unknown[0x3338 - sizeof(AtMaterialOwner)];
    AtGrD3DTextureDatabase* m_textureDatabases;  // +0x3338 linked through +0x54 of each, by 0x000EF0A0; not recovered here
    void* m_unknown333C;                         // +0x333C
    AtGrD3DMeshDatabase* m_meshDatabases;        // +0x3340 linked through m_next
};

// ---- Animations: .XKA -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// What an .XKA file's table says, at 0x10 in the file. 0x78 bytes. Offsets in the first block are from its start; in the second, likewise.
struct AtXkaHeader {
    unsigned int m_scratchSize;        // +0x00 the first block, read and freed: names and records
    unsigned int m_animationCount;     // +0x04
    unsigned int m_namesOffset;        // +0x08 eight bytes each, in the first block
    unsigned int m_recordsOffset;      // +0x0C 0x60 bytes each, in the first block
    unsigned int m_keyFlags;           // +0x10 added to the keys' flags, with kQuantised's neighbour 0x400
    unsigned int m_keyUnknown14;       // +0x14 to the keys' +0x40; their slot 26 multiplies 16-bit translations by it as a real
    unsigned int m_keyUnknown18;       // +0x18 to the keys' +0x44
    unsigned int m_keptSize;           // +0x1C the second block, which the database keeps: components and keys
    unsigned int m_rotationCount;      // +0x20 then the offset of each list in the second block
    unsigned int m_rotationOffset;     // +0x24
    unsigned int m_translationCount;   // +0x28
    unsigned int m_translationOffset;  // +0x2C
    unsigned int m_scaleCount;         // +0x30
    unsigned int m_scaleOffset;        // +0x34
    unsigned int m_noteCount;          // +0x38
    unsigned int m_noteOffset;         // +0x3C
    unsigned int m_cameraCount;        // +0x40
    unsigned int m_cameraOffset;       // +0x44
    unsigned int m_frameCount;         // +0x48 then the keys': count and offset, for frame numbers and each kind of key
    unsigned int m_frameOffset;        // +0x4C
    unsigned int m_rotationKeyCount;   // +0x50
    unsigned int m_rotationKeyOffset;  // +0x54
    unsigned int m_translationKeyCount;   // +0x58
    unsigned int m_translationKeyOffset;  // +0x5C
    unsigned int m_scaleKeyCount;      // +0x60
    unsigned int m_scaleKeyOffset;     // +0x64
    unsigned int m_noteKeyCount;       // +0x68
    unsigned int m_noteKeyOffset;      // +0x6C
    unsigned int m_cameraKeyCount;     // +0x70
    unsigned int m_cameraKeyOffset;    // +0x74
};

// One animation as an .XKA holds it. 0x60 bytes; the 16-bit fields of AtAnimation are 32 here.
struct AtXkaAnimation {
    unsigned int m_flags;              // +0x00
    float m_rate;                      // +0x04
    float m_velocity[3];               // +0x08
    float m_angularVelocity[3];        // +0x14
    unsigned int m_rotation[2];        // +0x20
    unsigned int m_translation[2];     // +0x28
    unsigned int m_scale[2];           // +0x30
    unsigned int m_note[2];            // +0x38
    unsigned int m_camera[2];          // +0x40
    unsigned int m_length;             // +0x48
    unsigned int m_loopPeriod;         // +0x4C
    float m_boundingSphere[4];         // +0x50
};

// The keys of an .XKA: pointers into the database's second block. Vftable 0x00289B38. Slots 1, 3 and 14 to 18 are recovered, 6 is the base's; the rest stop (src/at/AtGrD3DUnrecovered.cpp).
class AtGrD3DAnimationKeys : public AtAnimationKeys {
public:
    AtGrD3DAnimationKeys();            // 0x00101F60
    ~AtGrD3DAnimationKeys();           // 0x00102800 not virtual: the device calls it. Lists it was given by a file are not freed: only their pointers are cleared

    // 0x00101690. Counts and offsets from header; data is the block they are in. Frame numbers are the base set's count.
    void ReadBinary(const AtXkaHeader* header, char* data);

    void Allocate(unsigned int count) override;       // slot 0, 0x00101730
    void Free() override;                             // slot 1, 0x00101790
    void ReadEntries(AtIoTokenizer* tokenizer) override;  // slot 2, 0x001019D0
    void Clear() override;                            // slot 3, 0x00101F90
    void Interpolate(unsigned int first, unsigned int count, float time, int length, int* from, int* to, float* t) override;  // slot 4, 0x00101AC0
    int Find(unsigned int first, unsigned int count, float time, int length) override;  // slot 5, 0x00101BD0
    void Load() override;                             // slot 7, 0x00102450
    unsigned short At(unsigned int index) override;   // slot 8, 0x00101C50
    void Slot9(unsigned int count) override;   // 0x001017A0 these five allocate each kind of key
    void Slot10(unsigned int count) override;  // 0x00101810
    void Slot11(unsigned int count) override;  // 0x00101880
    void Slot12(unsigned int count) override;  // 0x001018F0
    void Slot13(unsigned int count) override;  // 0x00101960
    void Slot14() override;  // 0x00101800 these five free them
    void Slot15() override;  // 0x00101870
    void Slot16() override;  // 0x001018E0
    void Slot17() override;  // 0x00101950
    void Slot18() override;  // 0x001019C0
    void ReadRotationKeys(AtIoTokenizer* tokenizer) override;     // slot 19, 0x00101FE0
    void ReadTranslationKeys(AtIoTokenizer* tokenizer) override;  // slot 20, 0x001020B0
    void ReadScaleKeys(AtIoTokenizer* tokenizer) override;        // slot 21, 0x00102150
    void ReadNoteKeys(AtIoTokenizer* tokenizer) override;         // slot 22, 0x00101A00
    void ReadCameraKeys(AtIoTokenizer* tokenizer) override;       // slot 23, 0x00101A60
    virtual void Slot24();   // 0x001021F0
    virtual void Slot25();   // 0x00101C60
    virtual void Slot26();   // 0x001026F0 a translation key: 16 bits each, scaled by +0x40, when flag 0x1000 is set; else three reals

    unsigned short* m_frames;        // +0x28
    void* m_rotationKeys;            // +0x2C
    void* m_translationKeys;         // +0x30
    void* m_scaleKeys;               // +0x34
    void* m_noteKeys;                // +0x38
    void* m_cameraKeys;              // +0x3C
    unsigned int m_unknown40;        // +0x40 a scale for 16-bit translations, by slot 26
    unsigned int m_unknown44;        // +0x44
};

// Vftable 0x00289BB0. Has a binary form, always: an .XKA.
class AtGrD3DAnimationDatabase : public AtAnimationDatabase {
public:
    AtGrD3DAnimationDatabase();               // 0x001028D0
    ~AtGrD3DAnimationDatabase() override;     // slot 0; 0x00102D30 deleting, 0x001028F0 complete
    void Clear() override;                    // slot 1, 0x00102890 the second block, then the base's
    int HasBinaryForm() override;             // slot 2, 0x001A2B40 1, folded with others
    // Slot 3, 0x00102970. name + ".XKA": the table, the first block onto the token file's heap, the second onto the database's; keys from the device; animations by name. A failed open or read is reported and reading
    // goes on.
    void LoadBinaryForm(AtAnimationDevice* device, const char* name) override;

    char* m_data;                             // +0x4C the second block; the component lists and the keys point into it
};

// Vftable 0x0028663C. What an owner's m_animationDevice is on the Xbox: it makes the keys and the databases above.
class AtGrD3DAnimationDevice : public AtAnimationDevice {
public:
    AtGrD3DAnimationDevice();                 // 0x000C8920
    ~AtGrD3DAnimationDevice();                // 0x000C8930 not virtual
    AtAnimationKeys* NewKeys() override;                            // slot 0, 0x000C87E0 null is raised
    void FreeKeys(AtAnimationKeys* keys) override;                  // slot 1, 0x000C8940
    AtAnimationDatabase* NewDatabase() override;                    // slot 2, 0x000C8880 null is raised
    void FreeDatabase(AtAnimationDatabase* database) override;      // slot 3, 0x000EB9D0 through its virtual destructor; folded with other factories' frees

    static void* s_heap;                      // 0x003ADF7C
};

// ---- Meshes: .XGG -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// What an .XGG file's table says, at 0x10 in the file. 0x58 bytes. Offsets are in the first block (scratch, freed when read), the second (kept) or the third (memory for the graphics processor).
struct AtXggHeader {
    char m_nameSpace[8];               // +0x00 used as the name space when the first byte is not 0
    unsigned int m_scratchSize;        // +0x08
    unsigned int m_materialRefOffset;  // +0x0C scratch: the meshes' references to materials, 0x10 bytes each
    unsigned int m_meshCount;          // +0x10
    unsigned int m_meshOffset;         // +0x14 scratch: the meshes' records, 0x18 bytes each, a name at 0x0C
    unsigned int m_vertexArrayCount;   // +0x18
    unsigned int m_vertexArrayOffset;  // +0x1C scratch: 0x3C bytes each
    unsigned int m_indexArrayCount;    // +0x20
    unsigned int m_indexArrayOffset;   // +0x24 scratch: 8 bytes each
    unsigned int m_streamCount;        // +0x28
    unsigned int m_streamOffset;       // +0x2C scratch: 0x48 bytes each
    unsigned int m_keptSize;           // +0x30
    unsigned int m_triangleOffset;     // +0x34 kept: triangles, 0x10 bytes each
    unsigned int m_partOffset;         // +0x38 kept: parts of meshes, 0x20 bytes each
    unsigned int m_streamDataOffset;   // +0x3C kept
    unsigned int m_vertexArrayDataOffset;  // +0x40 kept
    unsigned int m_indexDataOffset;    // +0x44 kept
    unsigned int m_meshKeptOffset;     // +0x48 kept
    unsigned int m_contiguousSize;     // +0x4C 0: no third block
    unsigned int m_vertexDataOffset;   // +0x50 contiguous
    unsigned int m_meshContiguousOffset;  // +0x54 contiguous
};

// The items a mesh database makes. Their sizes are the original's; what is in them is not recovered: every body is in src/at/AtGrD3DUnrecovered.cpp and stops. Each reads its part of the blocks and moves the cursors it is
// given past it. The bases' slots are overridden only so that the classes can be made; the original's vector deleting destructors are the bases' slots named Slot2 (vertex array), Slot1 (index array) and Slot0 (mesh).

// Vftable 0x00288CF8, by way of a class whose constructor is 0x00118B10.
class AtGrD3DVertexArray : public AtVertexArray {
public:
    AtGrD3DVertexArray();              // 0x000EC890
    virtual ~AtGrD3DVertexArray();     // 0x000ED260
    void ReadBinary(char** records, char** kept, char** contiguous);  // 0x000EC8C0

    float* PositionOf(float* index) override;
    void Slot1() override;
    void Slot2() override;
    void Slot3() override;
    void Slot4() override;
    void Slot5() override;
    void Slot6() override;
    void Slot7() override;
    void Slot8() override;
    void Slot9() override;
    void Slot10() override;
    void Slot11() override;
    void Slot12() override;
    void Slot13() override;
    void Slot14() override;
    void Slot15() override;
    void Finish() override;
    void Slot17() override;
    void Slot18() override;
    void Slot19() override;
    void Slot20() override;
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, unsigned int index) override;

    char m_unknown0C[0x38];
    AtGrD3DOwner* m_owner;             // +0x44 set by the database as it makes them
    char m_unknown48[0x1C];
};

// Vftable 0x00289358.
class AtGrD3DIndexArray : public AtIndexArray {
public:
    AtGrD3DIndexArray();               // 0x000F8010
    virtual ~AtGrD3DIndexArray();      // 0x000F84B0
    void ReadBinary(char** records, char** kept);  // 0x000F7EE0

    void Allocate(unsigned int count, unsigned int flags) override;
    void Slot1() override;
    void Slot2() override;
    void Lock(void** out, int mode) override;
    void Unlock() override;
    void Finish() override;

    AtGrD3DOwner* m_owner;             // +0x18 set by the database as it makes them
    char m_unknown1C[0x08];
};

// Vftable 0x00288F08, by way of a class whose constructor is 0x0011A0D0. What it is, beyond a block of vertices of some stride, is not known.
class AtGrD3DVertexStream {
public:
    AtGrD3DVertexStream();             // 0x000EECA0
    virtual ~AtGrD3DVertexStream();    // 0x000EEF00
    void ReadBinary(AtMaterialOwner* owner, char** records, char** kept);  // 0x000EE750

    char m_unknown04[0x6C];
};

// Vftable 0x0028B918.
class AtGrD3DMesh : public AtMesh {
public:
    AtGrD3DMesh();                     // 0x00122460
    virtual ~AtGrD3DMesh();            // 0x00123390
    // 0x00122550. Reads one mesh and writes its name to name.
    void ReadBinary(AtMaterialOwner* owner, AtGrD3DMeshDatabase* database, char** triangles, char** parts, char** materials, char** records, char** kept, char** contiguous, char* name);

    void Slot0() override;
    void Slot1() override;
    AtVertexArray* OwnVertexArrayAt(unsigned int index) override;
    AtIndexArray* OwnIndexArrayAt(unsigned int index) override;
    void Slot4() override;
    void Slot5() override;
    void Slot6() override;
    void Slot7() override;
    void Slot8() override;
    void Slot9() override;
    void Slot10() override;
    void Slot11() override;
    void Read(AtMaterialOwner* owner, AtMeshDatabase* database, AtIoTokenizer* tokenizer, const char* ns) override;
    AtVertexArray* VertexArrayAt(unsigned int index) override;

    char m_unknown1C[0x34];
};
// Vftable 0x0028A730. Its binary form is an .XGG, always.
class AtGrD3DMeshDatabase : public AtMeshDatabase {
public:
    AtGrD3DMeshDatabase();                    // 0x001108B0
    ~AtGrD3DMeshDatabase() override;          // slot 0; 0x001125D0 deleting, 0x00111140 complete
    void Clear() override;                    // slot 1, 0x001108F0
    void MakeMeshes(AtMaterialOwner* owner) override;          // slot 2, 0x00110490 also puts this database on the owner's list
    void MakeVertexArrays(AtMaterialOwner* owner) override;    // slot 4, 0x00110E90
    void MakeIndexArrays(AtMaterialOwner* owner) override;     // slot 5, 0x00110F70
    int HasBinaryForm() override;                              // slot 6, 0x001A2B40 1, folded
    // Slot 7, 0x001109E0. name + ".XGG": the table, the three blocks, then the items, kind by kind, each reading its part. A failed open or read is reported and reading goes on.
    void LoadBinaryForm(AtMaterialOwner* owner, const char* name) override;
    void Slot8() override;                    // 0x00111050 makes m_more, a database of this one's mesh count and name space, with no file
    AtMesh* MeshAt(unsigned int index) override;               // slot 9, 0x00110470
    AtVertexArray* VertexArrayAt(unsigned int index) override; // slot 10, 0x00110880
    AtIndexArray* IndexArrayAt(unsigned int index) override;   // slot 11, 0x00110890

    void MakeStreams();                       // 0x00110680

    AtGrD3DOwner* m_owner;                    // +0x24 set by MakeMeshes, which puts this database on the owner's list; Clear takes it off
    AtGrD3DMesh* m_meshes;                    // +0x28
    AtGrD3DVertexArray* m_vertexArrays;       // +0x2C
    AtGrD3DIndexArray* m_indexArrays;         // +0x30
    AtGrD3DVertexStream* m_streams;           // +0x34
    unsigned int m_streamCount;               // +0x38
    char* m_kept;                             // +0x3C the second block
    unsigned int m_keptSize;                  // +0x40
    void* m_contiguous;                       // +0x44 the third block
    unsigned int m_contiguousSize;            // +0x48 the table's size, kept even when nothing was allocated
    AtGrD3DMeshDatabase* m_next;              // +0x4C on the owner's list. Clear leaves it

    static void* s_heap;                      // 0x002C637C for the second block and the items
};

// ---- The other kinds: classes with no bodies (src/at/AtGrD3DUnrecovered.cpp) ------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028A6E0, 0x58 bytes.
class AtGrD3DTextureDatabase : public AtTextureDatabase {
public:
    AtGrD3DTextureDatabase();                 // 0x0010FFC0
    ~AtGrD3DTextureDatabase() override;       // 0x001101D0
    void Clear() override;                    // 0x0010FFF0
    void OnReady(AtMaterialOwner* owner) override;  // 0x0010F810
    void MakeSurfaces2D() override;           // 0x0010F830
    void MakeSurfaces3D() override;           // 0x0010F950
    void MakeTextures2D() override;           // 0x0010FA70
    void MakeTextures3D() override;           // 0x0010FB90
    void MakeCubeMaps() override;             // 0x0010FCB0
    void MakeMore() override;                 // 0x001100D0
    void* Surface2DAt(unsigned int index) override;   // 0x0010FDD0
    void* Surface3DAt(unsigned int index) override;   // 0x0010FDF0
    AtTextureItem* Texture2DAt(unsigned int index) override;  // 0x0010FE10
    AtTextureItem* Texture3DAt(unsigned int index) override;  // 0x0010FE20
    AtTextureItem* CubeMapAt(unsigned int index) override;    // 0x0010FE40

    char m_unknown34[0x24];
};

// Vftable 0x00289300, 0x40 bytes.
class AtGrD3DMaterialDatabase : public AtMaterialDatabase {
public:
    AtGrD3DMaterialDatabase();                // 0x000F7790
    ~AtGrD3DMaterialDatabase() override;      // 0x000F7970
    void Clear() override;                    // 0x000F77B0
    void OnReady(AtMaterialOwner* owner) override;  // 0x000F79C0
    void Slot3() override;                    // 0x000F7AF0
    AtMaterial* MaterialAt(unsigned int index) override;  // 0x000F7840

    char m_unknown34[0x0C];
};

// Vftable 0x0028A720, 0x1C bytes.
class AtGrD3DShaderDatabase : public AtShaderDatabase {
public:
    AtGrD3DShaderDatabase();                  // 0x00110260
    ~AtGrD3DShaderDatabase() override;        // 0x001103F0
    void Clear() override;                    // 0x00110280
    void OnReady() override;                  // 0x001102D0
    AtShader* ShaderAt(unsigned int index) override;  // 0x00110220

    void* m_shaders;                          // +0x18
};

// Vftable 0x0028A764, 0x20 bytes.
class AtGrD3DLightDatabase : public AtLightDatabase {
public:
    AtGrD3DLightDatabase();                   // 0x001111B0
    ~AtGrD3DLightDatabase() override;         // 0x00111350
    void Clear() override;                    // 0x001111D0
    void MakeLights(AtMaterialOwner* owner) override;  // 0x00111210
    AtLight* LightAt(unsigned int index) override;     // 0x00111190

    void* m_lights;                           // +0x18
    AtMaterialOwner* m_owner;                 // +0x1C
};

// Vftable 0x0028A774, 0x3C bytes. Its slots 4 and 5 are a binary form, 0x001A2B40 and 0x00111630, which AtFontDatabase::Load never asks for; this class does not override them.
class AtGrD3DFontDatabase : public AtFontDatabase {
public:
    AtGrD3DFontDatabase();                    // 0x001115A0
    ~AtGrD3DFontDatabase() override;          // 0x001119B0
    void Clear() override;                    // 0x001115D0
    void OnReady(AtMaterialOwner* owner) override;  // 0x00111400
    void MakeFonts2D() override;              // 0x00111420
    AtFontItem* Font2DAt(unsigned int index) override;  // 0x001113E0

    char m_unknown28[0x14];
};

// Vftable 0x0028A794, 0x34 bytes.
class AtGrD3DImageDatabase : public AtImageDatabase {
public:
    AtGrD3DImageDatabase();                   // 0x00111BC0
    ~AtGrD3DImageDatabase() override;         // 0x00111C60
    void Clear() override;                    // 0x00111BE0
    void MakeItems() override;                // 0x00111A40
    void OnReady(AtMaterialOwner* owner) override;  // 0x00111A20
    AtImageItem* ImageAt(unsigned int index) override;  // 0x00111A00

    char m_unknown28[0x0C];
};

// Vftable 0x0028A7AC, 0x24 bytes.
class AtGrD3DVertexProcessDatabase : public AtVertexProcessDatabase {
public:
    AtGrD3DVertexProcessDatabase();           // 0x00111E50
    ~AtGrD3DVertexProcessDatabase() override; // 0x00111EC0
    void Clear() override;                    // 0x00111E70
    void Make(AtMaterialOwner* owner) override;     // 0x00111CC0
    AtProcessItem* ItemAt(unsigned int index) override;  // 0x00111CB0

    char m_unknown18[0x0C];
};

// Vftable 0x0028A7BC, 0x24 bytes.
class AtGrD3DPixelProcessDatabase : public AtPixelProcessDatabase {
public:
    AtGrD3DPixelProcessDatabase();            // 0x001120B0
    ~AtGrD3DPixelProcessDatabase() override;  // 0x00112120
    void Clear() override;                    // 0x001120D0
    void Make(AtMaterialOwner* owner) override;     // 0x00111F20
    AtProcessItem* ItemAt(unsigned int index) override;  // 0x00111F10

    char m_unknown18[0x0C];
};

// ---- The loader -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028A7D0. What a real build makes to load an .AGD: each kind's list becomes an array of the classes above, by count, from AtDatabaseLoader's heap. 0xD8 bytes.
class AtGrD3DDatabaseLoader : public AtDatabaseLoader {
public:
    AtGrD3DDatabaseLoader();                  // 0x00112D60
    ~AtGrD3DDatabaseLoader() override;        // slot 11; 0x00113090 deleting, 0x00112F70 complete

    void MakeTextures() override;             // slot 0, 0x00112170
    void MakeMaterials() override;            // slot 1, 0x001122A0
    void MakeShaders() override;              // slot 2, 0x001123D0
    void MakeMeshes() override;               // slot 3, 0x00112500
    void MakeLights() override;               // slot 4, 0x00112630
    void Slot5() override;                    // slot 5, 0x001CC9F0 empty, folded
    void MakeFonts() override;                // slot 6, 0x00112760
    void MakeImages() override;               // slot 7, 0x00112890
    void MakeVertexProcesses() override;      // slot 8, 0x001129C0
    void MakePixelProcesses() override;       // slot 9, 0x00112AF0
    void MakeLoaders() override;              // slot 10, 0x00112FC0 each constructor empties the shared lists of names: see AtDatabaseLoader
    void Unload() override;                   // slot 12, 0x00112DB0 its arrays, last kind first, then the base's
    AtTextureDatabase* TextureAt(unsigned int index) override;          // slot 13, 0x00112C20
    AtMaterialDatabase* MaterialAt(unsigned int index) override;        // slot 14, 0x00112C40
    AtShaderDatabase* ShaderAt(unsigned int index) override;            // slot 15, 0x00112C60
    AtMeshDatabase* MeshAt(unsigned int index) override;                // slot 16, 0x00112C80
    AtLightDatabase* LightAt(unsigned int index) override;              // slot 17, 0x00112CA0
    AtFontDatabase* FontAt(unsigned int index) override;                // slot 18, 0x00112CC0
    AtImageDatabase* ImageAt(unsigned int index) override;              // slot 19, 0x00112CE0
    AtVertexProcessDatabase* VertexProcessAt(unsigned int index) override;  // slot 20, 0x00112D00
    AtPixelProcessDatabase* PixelProcessAt(unsigned int index) override;    // slot 21, 0x00112D20
    AtDatabaseLoader* LoaderAt(unsigned int index) override;            // slot 22, 0x00112D40

    AtGrD3DTextureDatabase* m_textures;       // +0xB0
    AtGrD3DShaderDatabase* m_shaders;         // +0xB4
    AtGrD3DMaterialDatabase* m_materials;     // +0xB8
    AtGrD3DMeshDatabase* m_meshes;            // +0xBC
    AtGrD3DLightDatabase* m_lights;           // +0xC0
    AtGrD3DFontDatabase* m_fonts;             // +0xC4
    AtGrD3DImageDatabase* m_images;           // +0xC8
    AtGrD3DVertexProcessDatabase* m_vertexProcesses;  // +0xCC
    AtGrD3DPixelProcessDatabase* m_pixelProcesses;    // +0xD0
    AtGrD3DDatabaseLoader* m_loaders;         // +0xD4
};

// ---- The platform's factory -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Two slots of a table of small factories, which make an object from a heap of their own and raise on failure, paired with frees that delete through slot 0 or 2. They are at 0x00288A98 and 0x00288C54; where the
// table starts, and so their slot numbers and the class, are not established, and `this` is not read. The .GOD loader, 0x0013AD70, reaches the first.
class AtXboxFactory {
public:
    AtSoXboxDatabase* NewSoundDatabase();      // 0x000E9E90
    AtGrD3DDatabaseLoader* NewDatabaseLoader(); // 0x000EBB50

    static void* s_soundHeap;                 // 0x003AE370
    static void* s_loaderHeap;                // 0x003B13CC
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtMaterialOwner) == 0x39C, "AtMaterialOwner");
static_assert(offsetof(AtGrD3DOwner, m_textureDatabases) == 0x3338, "AtGrD3DOwner::m_textureDatabases");
static_assert(offsetof(AtGrD3DOwner, m_meshDatabases) == 0x3340, "AtGrD3DOwner::m_meshDatabases");
static_assert(sizeof(AtXkaHeader) == 0x78, "AtXkaHeader");
static_assert(sizeof(AtXkaAnimation) == 0x60, "AtXkaAnimation");
static_assert(sizeof(AtXggHeader) == 0x58, "AtXggHeader");
static_assert(offsetof(AtGrD3DAnimationKeys, m_frames) == 0x28, "AtGrD3DAnimationKeys::m_frames");
static_assert(sizeof(AtGrD3DAnimationKeys) == 0x48, "AtGrD3DAnimationKeys");
static_assert(sizeof(AtGrD3DAnimationDatabase) == 0x50, "AtGrD3DAnimationDatabase");
static_assert(sizeof(AtGrD3DVertexArray) == 0x64, "AtGrD3DVertexArray");
static_assert(offsetof(AtGrD3DVertexArray, m_owner) == 0x44, "AtGrD3DVertexArray::m_owner");
static_assert(sizeof(AtGrD3DIndexArray) == 0x24, "AtGrD3DIndexArray");
static_assert(offsetof(AtGrD3DIndexArray, m_owner) == 0x18, "AtGrD3DIndexArray::m_owner");
static_assert(sizeof(AtGrD3DVertexStream) == 0x70, "AtGrD3DVertexStream");
static_assert(sizeof(AtGrD3DMesh) == 0x50, "AtGrD3DMesh");
static_assert(offsetof(AtGrD3DMeshDatabase, m_owner) == 0x24, "AtGrD3DMeshDatabase::m_owner");
static_assert(offsetof(AtGrD3DMeshDatabase, m_next) == 0x4C, "AtGrD3DMeshDatabase::m_next");
static_assert(sizeof(AtGrD3DMeshDatabase) == 0x50, "AtGrD3DMeshDatabase");
static_assert(sizeof(AtGrD3DTextureDatabase) == 0x58, "AtGrD3DTextureDatabase");
static_assert(sizeof(AtGrD3DMaterialDatabase) == 0x40, "AtGrD3DMaterialDatabase");
static_assert(sizeof(AtGrD3DShaderDatabase) == 0x1C, "AtGrD3DShaderDatabase");
static_assert(sizeof(AtGrD3DLightDatabase) == 0x20, "AtGrD3DLightDatabase");
static_assert(sizeof(AtGrD3DFontDatabase) == 0x3C, "AtGrD3DFontDatabase");
static_assert(sizeof(AtGrD3DImageDatabase) == 0x34, "AtGrD3DImageDatabase");
static_assert(sizeof(AtGrD3DVertexProcessDatabase) == 0x24, "AtGrD3DVertexProcessDatabase");
static_assert(sizeof(AtGrD3DPixelProcessDatabase) == 0x24, "AtGrD3DPixelProcessDatabase");
static_assert(offsetof(AtGrD3DDatabaseLoader, m_textures) == 0xB0, "AtGrD3DDatabaseLoader::m_textures");
static_assert(offsetof(AtGrD3DDatabaseLoader, m_loaders) == 0xD4, "AtGrD3DDatabaseLoader::m_loaders");
static_assert(sizeof(AtGrD3DDatabaseLoader) == 0xD8, "AtGrD3DDatabaseLoader");
#pragma pop_macro("offsetof")
#endif
