// Not recovered code. The bodies of what include/at/AtGrD3DDatabase.h and include/at/AtGrD3DTextures.h declare and is not recovered: the six kinds of database other than meshes, animations, textures and materials,
// the items a mesh database makes, what a texture database holds that loading does not need, and the keys' own slots. Each goes to AtGrD3DUnrecovered, or for an item's reader to a function of its own, in src/at/AtGrD3DSeam.cpp, which stops with a diagnostic; a test supplies those instead. Each body names the
// original's function, or for a slot whose function is not known, the address of its entry in the vftable.
#include "at/AtGrD3DDatabase.h"

#define STOP(va) AtGrD3DUnrecovered(va, this, 0, 0)
#define STOP1(va, a) AtGrD3DUnrecovered(va, this, (unsigned int)(a), 0)
#define POINTER(T, va, a) ((T*)(unsigned long long)AtGrD3DUnrecovered(va, this, (unsigned int)(a), 0))

// ---- What the texture database holds and does not recover: include/at/AtGrD3DTextures.h ---------------------------------------------------------------------------------------------------------------------------

void AtGrD3DPalette::CopyFrom(const AtGrIPalette* from) { STOP1(0x000F5AE0, from != 0); }
void AtGrD3DPalette::Slot2() { STOP(0x000F58E0); }
void AtGrD3DPalette::Slot3() { STOP(0x000F5EC0); }
void AtGrD3DPalette::Slot4() { STOP(0x000F5920); }
void AtGrD3DPalette::Slot5() { STOP(0x000F5F20); }
void AtGrD3DPalette::Slot6() { STOP(0x000F5970); }
void AtGrD3DPalette::Slot7() { STOP(0x000F5B10); }
void AtGrD3DPalette::Slot8() { STOP(0x000F5B80); }
void AtGrD3DPalette::Slot9() { STOP(0x000F5BF0); }
void AtGrD3DPalette::Slot10() { STOP(0x000F5C60); }

void AtGrD3DSurface2D::Slot6() { STOP(0x000FD240); }
void AtGrD3DSurface2D::Slot7() { STOP(0x000FD9A0); }
void AtGrD3DSurface2D::Slot8() { STOP(0x0005F2E0); }
void AtGrD3DSurface2D::Slot9() { STOP(0x000FE260); }
void AtGrD3DSurface2D::Slot10() { STOP(0x000FE320); }
void AtGrD3DSurface2D::Slot11() { STOP(0x001ACD70); }
void AtGrD3DSurface2D::Slot12() { STOP(0x000FD0C0); }
void AtGrD3DSurface2D::Slot13() { STOP(0x000FD190); }

// Slots 8 and 9 return pointers, which the base's type, an int, cannot hold on a 64-bit host: Texture() and PaletteOf() do what they do.
int AtGrD3DTexture2D::Unknown8() { return (int)STOP(0x000FC620); }
int AtGrD3DTexture2D::Unknown9() { return (int)STOP(0x000FC590); }
void AtGrD3DTexture2D::Slot11() { STOP(0x000FC8A0); }

AtGrD3DTexture3D::AtGrD3DTexture3D() { STOP(0x001214A0); }
AtGrD3DTexture3D::~AtGrD3DTexture3D() { STOP(0x001214C0); }
void AtGrD3DTexture3D::Slot11() { STOP(0x0028B8B8); }
void AtGrD3DTexture3D::OnLoaded(AtMaterialOwner* owner) { STOP1(0x0028B8BC, owner != 0); }

AtGrD3DCubeMap::AtGrD3DCubeMap() { STOP(0x00121730); }
AtGrD3DCubeMap::~AtGrD3DCubeMap() { STOP(0x001220E0); }
void AtGrD3DCubeMap::Slot11() { STOP(0x00121920); }
void AtGrD3DCubeMap::OnLoaded(AtMaterialOwner* owner) { STOP1(0x00121F60, owner != 0); }

AtGrD3DSurface3D::AtGrD3DSurface3D() { STOP(0x000FCD50); }
AtGrD3DSurface3D::~AtGrD3DSurface3D() { STOP(0x000FCDF0); }
int AtGrD3DSurface3D::Slot3() { return (int)STOP(0x00289744); }

// ---- Shaders, lights ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DShaderDatabase::AtGrD3DShaderDatabase() { STOP(0x00110260); }
AtGrD3DShaderDatabase::~AtGrD3DShaderDatabase() { STOP(0x001103F0); }
void AtGrD3DShaderDatabase::Clear() { STOP(0x00110280); }
void AtGrD3DShaderDatabase::OnReady() { STOP(0x001102D0); }
AtShader* AtGrD3DShaderDatabase::ShaderAt(unsigned int index) { return POINTER(AtShader, 0x00110220, index); }

AtGrD3DLightDatabase::AtGrD3DLightDatabase() { STOP(0x001111B0); }
AtGrD3DLightDatabase::~AtGrD3DLightDatabase() { STOP(0x00111350); }
void AtGrD3DLightDatabase::Clear() { STOP(0x001111D0); }
void AtGrD3DLightDatabase::MakeLights(AtMaterialOwner* owner) { STOP1(0x00111210, owner != 0); }
AtLight* AtGrD3DLightDatabase::LightAt(unsigned int index) { return POINTER(AtLight, 0x00111190, index); }

// ---- Fonts, images, processes ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DFontDatabase::AtGrD3DFontDatabase() { STOP(0x001115A0); }
AtGrD3DFontDatabase::~AtGrD3DFontDatabase() { STOP(0x001119B0); }
void AtGrD3DFontDatabase::Clear() { STOP(0x001115D0); }
void AtGrD3DFontDatabase::OnReady(AtMaterialOwner* owner) { STOP1(0x00111400, owner != 0); }
void AtGrD3DFontDatabase::MakeFonts2D() { STOP(0x00111420); }
AtGrI2DFont* AtGrD3DFontDatabase::Font2DAt(unsigned int index) { return POINTER(AtGrI2DFont, 0x001113E0, index); }

AtGrD3DImageDatabase::AtGrD3DImageDatabase() { STOP(0x00111BC0); }
AtGrD3DImageDatabase::~AtGrD3DImageDatabase() { STOP(0x00111C60); }
void AtGrD3DImageDatabase::Clear() { STOP(0x00111BE0); }
void AtGrD3DImageDatabase::MakeItems() { STOP(0x00111A40); }
void AtGrD3DImageDatabase::OnReady(AtMaterialOwner* owner) { STOP1(0x00111A20, owner != 0); }
AtImage* AtGrD3DImageDatabase::ImageAt(unsigned int index) { return POINTER(AtImage, 0x00111A00, index); }

AtGrD3DVertexProcessDatabase::AtGrD3DVertexProcessDatabase() { STOP(0x00111E50); }
AtGrD3DVertexProcessDatabase::~AtGrD3DVertexProcessDatabase() { STOP(0x00111EC0); }
void AtGrD3DVertexProcessDatabase::Clear() { STOP(0x00111E70); }
void AtGrD3DVertexProcessDatabase::Make(AtMaterialOwner* owner) { STOP1(0x00111CC0, owner != 0); }
AtProcessItem* AtGrD3DVertexProcessDatabase::ItemAt(unsigned int index) { return POINTER(AtProcessItem, 0x00111CB0, index); }

AtGrD3DPixelProcessDatabase::AtGrD3DPixelProcessDatabase() { STOP(0x001120B0); }
AtGrD3DPixelProcessDatabase::~AtGrD3DPixelProcessDatabase() { STOP(0x00112120); }
void AtGrD3DPixelProcessDatabase::Clear() { STOP(0x001120D0); }
void AtGrD3DPixelProcessDatabase::Make(AtMaterialOwner* owner) { STOP1(0x00111F20, owner != 0); }
AtProcessItem* AtGrD3DPixelProcessDatabase::ItemAt(unsigned int index) { return POINTER(AtProcessItem, 0x00111F10, index); }

// ---- A mesh database's items -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DVertexArray::AtGrD3DVertexArray() { STOP(0x000EC890); }
AtGrD3DVertexArray::~AtGrD3DVertexArray() { STOP(0x000ED260); }
void AtGrD3DVertexArray::ReadBinary(char** records, char** kept, char** contiguous) { AtGrD3DReadVertexArray(this, records, kept, contiguous); }
float* AtGrD3DVertexArray::PositionOf(float* index) { return POINTER(float, 0x00288CF8, index != 0); }
void AtGrD3DVertexArray::Slot1() { STOP(0x00288CFC); }
void AtGrD3DVertexArray::Destroy(unsigned int) { STOP(0x00288D00); }
void AtGrD3DVertexArray::Slot3() { STOP(0x00288D04); }
void AtGrD3DVertexArray::Slot4() { STOP(0x00288D08); }
void AtGrD3DVertexArray::Slot5() { STOP(0x00288D0C); }
void AtGrD3DVertexArray::Slot6() { STOP(0x00288D10); }
void AtGrD3DVertexArray::Slot7() { STOP(0x00288D14); }
void AtGrD3DVertexArray::Slot8() { STOP(0x00288D18); }
void AtGrD3DVertexArray::Slot9() { STOP(0x00288D1C); }
void AtGrD3DVertexArray::Slot10() { STOP(0x00288D20); }
void AtGrD3DVertexArray::Slot11() { STOP(0x00288D24); }
void AtGrD3DVertexArray::Slot12() { STOP(0x00288D28); }
void AtGrD3DVertexArray::Slot13() { STOP(0x00288D2C); }
void AtGrD3DVertexArray::Slot14() { STOP(0x00288D30); }
void AtGrD3DVertexArray::Slot15() { STOP(0x00288D34); }
void AtGrD3DVertexArray::Finish() { STOP(0x00288D38); }
void AtGrD3DVertexArray::Slot17() { STOP(0x00288D3C); }
void AtGrD3DVertexArray::Slot18() { STOP(0x00288D40); }
void AtGrD3DVertexArray::Slot19() { STOP(0x00288D44); }
void AtGrD3DVertexArray::Slot20() { STOP(0x00288D48); }
void AtGrD3DVertexArray::Read(AtMaterialOwner*, AtIoTokenizer*, unsigned int index) { STOP1(0x00288D4C, index); }

AtGrD3DIndexArray::AtGrD3DIndexArray() { STOP(0x000F8010); }
AtGrD3DIndexArray::~AtGrD3DIndexArray() { STOP(0x000F84B0); }
void AtGrD3DIndexArray::ReadBinary(char** records, char** kept) { AtGrD3DReadIndexArray(this, records, kept); }
void AtGrD3DIndexArray::Allocate(unsigned int count, unsigned int) { STOP1(0x00289358, count); }
void AtGrD3DIndexArray::Destroy(unsigned int) { STOP(0x0028935C); }
void AtGrD3DIndexArray::Slot2() { STOP(0x00289360); }
void AtGrD3DIndexArray::Lock(void** out, int mode) { *out = POINTER(void, 0x00289364, mode); }
void AtGrD3DIndexArray::Unlock() { STOP(0x00289368); }
void AtGrD3DIndexArray::Finish() { STOP(0x0028936C); }

AtGrD3DVertexStream::AtGrD3DVertexStream() { STOP(0x000EECA0); }
AtGrD3DVertexStream::~AtGrD3DVertexStream() { STOP(0x000EEF00); }
void AtGrD3DVertexStream::ReadBinary(AtMaterialOwner* owner, char** records, char** kept) { AtGrD3DReadVertexStream(this, owner, records, kept); }

AtGrD3DMesh::AtGrD3DMesh() { STOP(0x00122460); }
AtGrD3DMesh::~AtGrD3DMesh() { STOP(0x00123390); }
void AtGrD3DMesh::ReadBinary(AtMaterialOwner* owner, AtGrD3DMeshDatabase* database, char** triangles, char** parts, char** materials, char** records, char** kept, char** contiguous, char* name) {
    AtGrD3DReadMesh(this, owner, database, triangles, parts, materials, records, kept, contiguous, name);
}
void AtGrD3DMesh::Slot0() { STOP(0x0028B918); }
void AtGrD3DMesh::Slot1() { STOP(0x0028B91C); }
AtVertexArray* AtGrD3DMesh::OwnVertexArrayAt(unsigned int index) { return POINTER(AtVertexArray, 0x0028B920, index); }
AtIndexArray* AtGrD3DMesh::OwnIndexArrayAt(unsigned int index) { return POINTER(AtIndexArray, 0x0028B924, index); }
void AtGrD3DMesh::Slot4() { STOP(0x0028B928); }
void AtGrD3DMesh::Slot5() { STOP(0x0028B92C); }
void AtGrD3DMesh::Slot6() { STOP(0x0028B930); }
void AtGrD3DMesh::Slot7() { STOP(0x0028B934); }
void AtGrD3DMesh::Slot8() { STOP(0x0028B938); }
void AtGrD3DMesh::Slot9() { STOP(0x0028B93C); }
void AtGrD3DMesh::Slot10() { STOP(0x0028B940); }
void AtGrD3DMesh::Slot11() { STOP(0x0028B944); }
void AtGrD3DMesh::Read(AtMaterialOwner*, AtMeshDatabase*, AtIoTokenizer*, const char*) { STOP(0x0028B948); }
AtVertexArray* AtGrD3DMesh::VertexArrayAt(unsigned int index) { return POINTER(AtVertexArray, 0x0028B94C, index); }

// ---- The keys' own slots -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DAnimationKeys::Allocate(unsigned int count) { STOP1(0x00101730, count); }
void AtGrD3DAnimationKeys::ReadEntries(AtIoTokenizer*) { STOP(0x001019D0); }
void AtGrD3DAnimationKeys::Interpolate(unsigned int first, unsigned int, float, int, int*, int*, float*) { STOP1(0x00101AC0, first); }
int AtGrD3DAnimationKeys::Find(unsigned int first, unsigned int, float, int) { return (int)STOP1(0x00101BD0, first); }
void AtGrD3DAnimationKeys::Load() { STOP(0x00102450); }
unsigned short AtGrD3DAnimationKeys::At(unsigned int index) { return (unsigned short)STOP1(0x00101C50, index); }
void AtGrD3DAnimationKeys::Slot9(unsigned int count) { STOP1(0x001017A0, count); }
void AtGrD3DAnimationKeys::Slot10(unsigned int count) { STOP1(0x00101810, count); }
void AtGrD3DAnimationKeys::Slot11(unsigned int count) { STOP1(0x00101880, count); }
void AtGrD3DAnimationKeys::Slot12(unsigned int count) { STOP1(0x001018F0, count); }
void AtGrD3DAnimationKeys::Slot13(unsigned int count) { STOP1(0x00101960, count); }
void AtGrD3DAnimationKeys::ReadRotationKeys(AtIoTokenizer*) { STOP(0x00101FE0); }
void AtGrD3DAnimationKeys::ReadTranslationKeys(AtIoTokenizer*) { STOP(0x001020B0); }
void AtGrD3DAnimationKeys::ReadScaleKeys(AtIoTokenizer*) { STOP(0x00102150); }
void AtGrD3DAnimationKeys::ReadNoteKeys(AtIoTokenizer*) { STOP(0x00101A00); }
void AtGrD3DAnimationKeys::ReadCameraKeys(AtIoTokenizer*) { STOP(0x00101A60); }
void AtGrD3DAnimationKeys::Slot24() { STOP(0x001021F0); }
void AtGrD3DAnimationKeys::Slot25() { STOP(0x00101C60); }
void AtGrD3DAnimationKeys::Slot26() { STOP(0x001026F0); }
