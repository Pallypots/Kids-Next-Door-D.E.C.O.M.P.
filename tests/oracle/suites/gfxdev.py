# Mutants of the graphics device: its abstract layers, the concrete device's construction and slots, and its members' set-up and freeing. Each text occurs exactly once in its file.
B = "AtMaterialOwnerBase.cpp"
G = "AtGraphicsDevice.cpp"
I = "AtIndexArray.cpp"

_DL_WORDS = ("    m_unknown04 = 0;\n    m_unknown08 = 0;\n    m_block0C = 0;\n    m_unknown10 = 0;\n    m_unknown14 = 0;\n    m_block18 = 0;\n    m_unknown1C = 0;\n    m_unknown20 = 0;\n"
             "    m_block24 = 0;\n    m_unknown28 = 0;\n}\n\n// ---- the concrete index array")
_BL_FREE = ("    if (m_block08 != 0) {\n        operator delete(m_block08);\n    }\n    if (m_block00 != 0) {\n        operator delete(m_block00);\n    }\n    m_block00 = 0;\n    m_unknown04 = 0;\n"
            "    m_block08 = 0;\n    m_unknown0C = 0;\n    m_block10 = 0;\n")
_PAIR_FREE = ("    if (m_block08 != 0) {\n        operator delete(m_block08);\n    }\n    if (m_block00 != 0) {\n        operator delete(m_block00);\n    }\n    m_block00 = 0;\n    m_unknown04 = 0;\n"
              "    m_block08 = 0;\n    m_unknown0C = 0;\n}")
_DL_FREE = "    if (m_block24 != 0) {\n        operator delete(m_block24);\n    }\n    if (m_block18 != 0) {\n        operator delete(m_block18);\n    }\n"
_IL_FREE = "    if (m_block1C != 0) {\n        operator delete(m_block1C);\n    }\n    if (m_block14 != 0) {\n        operator delete(m_block14);\n    }\n"


def _loop(member, count, fewer):
    return ("    for (int i = 0; i < %s; i++) {\n        %s[i]" % (count, member), "    for (int i = 0; i < %s; i++) {\n        %s[i]" % (fewer, member))


MUTANTS = [
    # The display mode.
    ("mode constructor: width left", B, "AtDisplayMode::AtDisplayMode() {\n    m_width = 0;\n", "AtDisplayMode::AtDisplayMode() {\n"),
    ("mode constructor: +0x20 left", B, "    m_unknown20 = 0;\n}\n\nvoid AtDisplayMode::Clear()", "}\n\nvoid AtDisplayMode::Clear()"),
    ("mode clear: format kept", B, "    m_format.Clear();\n", ""),
    ("mode clear: height kept", B, "    m_format.Clear();\n    m_width = 0;\n    m_height = 0;\n", "    m_format.Clear();\n    m_width = 0;\n"),
    ("mode clear: +0x1C kept", B, "    m_unknown1C = 0;\n    m_unknown20 = 0;\n}\n\n// ---- the display device", "    m_unknown20 = 0;\n}\n\n// ---- the display device"),

    # The display device.
    ("display device constructor: no reset", B, "    AtDisplayDevice::Slot1();  // Inline", "    // Inline"),
    ("display device deleting destructor: not freed", B, "    this->~AtDisplayDevice();\n    operator delete(this);\n", "    this->~AtDisplayDevice();\n"),
    ("display device reset: +4 kept", B, "    m_unknown04 = 0;\n    m_mode.Clear();\n", "    m_mode.Clear();\n"),
    ("display device reset: mode kept", B, "    m_unknown04 = 0;\n    m_mode.Clear();\n", "    m_unknown04 = 0;\n"),
    ("display device reset: no textures kept", B, "    m_noTextures = 0;\n", ""),
    ("display device reset: +0x33 kept", B, "    m_unknown31[2] = 0;\n", ""),
    ("display device reset: +0x2C 0x7F", B, "    m_unknown2C[0] = 0x80;\n", "    m_unknown2C[0] = 0x7F;\n"),
    ("display device reset: +0x2F kept", B, "    m_unknown2C[3] = 0x80;\n", ""),

    # The owner.
    ("owner constructor: +0x34 left", B, "AtMaterialOwner::AtMaterialOwner() {\n    m_textureLimits = 0;\n", "AtMaterialOwner::AtMaterialOwner() {\n"),
    ("owner constructor: +0x3A4 left", B, "    m_unknown3A4 = 0;\n}\n\nAtMaterialOwner::~AtMaterialOwner()", "}\n\nAtMaterialOwner::~AtMaterialOwner()"),
    ("owner destructor: no reset", B, "AtMaterialOwner::~AtMaterialOwner() {\n    AtMaterialOwner::Slot1();\n}", "AtMaterialOwner::~AtMaterialOwner() {\n}"),
    ("owner deleting destructor: not freed", B, "    this->~AtMaterialOwner();\n    operator delete(this);\n", "    this->~AtMaterialOwner();\n"),
    ("owner reset: images and fonts in the other order", B, "    ClearThroughTable(&m_imageSpaces);\n    m_images.Reset();\n    ClearThroughTable(&m_fontSpaces);\n",
     "    ClearThroughTable(&m_fontSpaces);\n    m_images.Reset();\n    ClearThroughTable(&m_imageSpaces);\n"),
    ("owner reset: +0x12C table after the lights", B, "    ClearThroughTable(&m_unknown12CSpaces);\n    m_unknown12C.Reset();\n    ClearThroughTable(&m_lightSpaces);\n",
     "    ClearThroughTable(&m_lightSpaces);\n    m_unknown12C.Reset();\n    ClearThroughTable(&m_unknown12CSpaces);\n"),
    ("owner reset: light animation spaces kept", B, "    ClearThroughTable(&m_lightAnimationSpaces);\n", ""),
    ("owner reset: pixel processes kept", B, "    m_pixelProcesses.Reset();\n", ""),
    ("owner reset: mirrors kept", B, "    m_mirrors.Reset();\n", ""),
    ("owner reset: textures kept", B, "    m_textures.Reset();\n    m_textureLimits = 0;\n", "    m_textureLimits = 0;\n"),
    ("owner reset: texture spaces kept", B, "    ClearThroughTable(&m_textureSpaces);\n", ""),
    ("owner reset: +0x34 kept", B, "    m_textures.Reset();\n    m_textureLimits = 0;\n", "    m_textures.Reset();\n"),
    ("owner reset: +0x3A0 kept", B, "    m_unknown3A0 = 0;\n    m_unknown3A4 = 0;\n    AtDisplayDevice::Slot1();\n", "    m_unknown3A4 = 0;\n    AtDisplayDevice::Slot1();\n"),
    ("owner reset: base not reset", B, "    m_unknown3A4 = 0;\n    AtDisplayDevice::Slot1();\n}", "    m_unknown3A4 = 0;\n}"),

    # The index array's bases.
    ("geometry array: count left", I, "    m_count = 0;\n", ""),
    ("geometry array: flags left", I, "    m_flags = 0;\n", ""),
    ("geometry array: load flags left", I, "    m_loadFlags = 0;  // One", "    // One"),
    ("geometry array: +0x0B left", I, "    m_unknown0B = 0;\n", ""),
    ("index array: index left", I, "    m_index = 0;\n", ""),
    ("index array: name left", I, "    m_name[0] = 0;\n", ""),

    # The context.
    ("context part 30: 0.5", G, "    m_unknown14 = 1.0f;\n}\n\nAtGfxContextBase::Part48", "    m_unknown14 = 0.5f;\n}\n\nAtGfxContextBase::Part48"),
    ("context part 30: +0x10 left", G, "AtGfxContextBase::Part30::Part30() {\n    m_unknown10 = 0;\n", "AtGfxContextBase::Part30::Part30() {\n"),
    ("context part 48: first block short",) + (G,) + _loop("m_unknown00", "0x18", "0x17"),
    ("context part 48: second block short",) + (G,) + _loop("m_unknown60", "0x1B", "0x1A"),
    ("context part 48: 4 at +0xD0", G, "    m_unknownD0 = 5;\n", "    m_unknownD0 = 4;\n"),
    ("context part 48: +0xDC positive", G, "    m_unknownDC = -1.0f;\n", "    m_unknownDC = 1.0f;\n"),
    ("context part 48: +0xE8 left", G, "    m_unknownE8 = 0;\n", ""),
    ("context part 48: +0xCC left", G, "    m_unknownCC = 0;\n", ""),
    ("context base constructor: no reset", G, "AtGfxContextBase::AtGfxContextBase() {\n    ResetBase();\n}", "AtGfxContextBase::AtGfxContextBase() {\n}"),
    ("context base destructor: no reset", G, "AtGfxContextBase::~AtGfxContextBase() {\n    ResetBase();\n}", "AtGfxContextBase::~AtGfxContextBase() {\n}"),
    ("context reset base: +0x10 kept", G, "void AtGfxContextBase::ResetBase() {\n    m_unknown10 = 0;\n", "void AtGfxContextBase::ResetBase() {\n"),
    ("context reset base: shared word kept", G, "    s_shared[1] = 0;\n", ""),
    ("context reset base: shared not pointed at", G, "    m_shared = s_shared;\n", ""),
    ("context reset base: alpha 0", G, "    m_color20[3] = 0xFF;\n", "    m_color20[3] = 0;\n"),
    ("context reset base: +0x24 0", G, "    m_unknown24 = 1.0f;\n", "    m_unknown24 = 0.0f;\n"),
    ("context reset base: +0x134 kept", G, "    m_unknown134 = 0;\n", ""),
    ("context reset base: +0x2948 kept", G, "    m_unknown2948 = 0;\n", ""),
    ("context constructor: no local reset", G, "AtGfxContext::AtGfxContext() {\n    ResetLocal();\n}", "AtGfxContext::AtGfxContext() {\n}"),
    ("context destructor: always unlinked", G, "    if (m_unknown10 != 0) {\n        m_device->UnlinkContext(this);\n    }\n", "    m_device->UnlinkContext(this);\n"),
    ("context destructor: never unlinked", G, "    if (m_unknown10 != 0) {\n        m_device->UnlinkContext(this);\n    }\n", ""),
    # Not a mutant: dropping AtGfxContextBase::Reset() from the destructor is equivalent, since the base's destructor runs ResetBase after ResetLocal, which reads nothing of the base's.
    ("context destructor: no local reset", G, "    AtGfxContextBase::Reset();\n    ResetLocal();\n}", "    AtGfxContextBase::Reset();\n}"),
    ("context reset local: next kept", G, "    m_unknown2958 = 0;\n    m_next = 0;\n", "    m_unknown2958 = 0;\n"),
    ("context reset local: device kept", G, "    m_device = 0;\n    m_unknown2954", "    m_unknown2954"),
    ("context reset local: +0x2964 kept", G, "    m_unknown2964 = 0;\n", ""),
    ("context reset local: +0x2984 short",) + (G,) + _loop("m_unknown2984", "0x15", "0x14"),
    ("context reset local: +0x29D8 short",) + (G,) + _loop("m_unknown29D8", "0x15", "0x14"),
    ("context reset local: +0x2A30 short",) + (G,) + _loop("m_unknown2A30", "7", "6"),
    ("context reset local: 1 at +0x2A60", G, "    m_unknown2A60 = 2;\n", "    m_unknown2A60 = 1;\n"),
    ("context reset local: +0x2A74 0", G, "    m_unknown2A74 = 1.0f;\n", "    m_unknown2A74 = 0.0f;\n"),
    ("context reset local: +0x2A78 kept", G, "    m_unknown2A78 = 0;\n", ""),

    # The state cache.
    ("state cache constructor: no reset", G, "AtGfxStateCache::AtGfxStateCache() {\n    Reset();\n}", "AtGfxStateCache::AtGfxStateCache() {\n}"),
    ("state cache destructor: block not freed", G, "    if (m_blockA58 != 0) {\n        operator delete(m_blockA58);\n    }\n    Reset();\n", "    Reset();\n"),
    ("state cache destructor: no reset", G, "        operator delete(m_blockA58);\n    }\n    Reset();\n}", "        operator delete(m_blockA58);\n    }\n}"),
    ("state cache reset: block kept", G, "    m_blockA58 = 0;\n    ResetState();\n", "    ResetState();\n"),
    ("state cache reset: no state reset", G, "    m_blockA58 = 0;\n    ResetState();\n", "    m_blockA58 = 0;\n"),
    ("state cache reset: +0x9B0 kept", G, "    m_unknown9B0 = 0;\n", ""),
    ("state cache reset: +0x9B4 short",) + (G,) + _loop("m_unknown9B4", "0x11", "0x10"),
    ("state cache reset: +0xA98 kept", G, "    m_unknownA98 = 0;\n", ""),
    ("state cache reset: +0x8 kept", G, "    m_unknown000[2] = 0;\n", ""),
    ("state cache reset state: +0xC short",) + (G,) + _loop("m_unknown00C", "0x160", "0x15F"),
    ("state cache reset state: +0x5AC short",) + (G,) + _loop("m_unknown5AC", "0x100", "0xFF"),
    ("state cache reset state: +0xA64 short",) + (G,) + _loop("m_unknownA64", "0xC", "0xB"),
    ("state cache reset state: +0x58C short",) + (G,) + _loop("m_unknown58C", "8", "7"),
    ("state cache reset state: +0xA0C kept", G, "    m_cachedA0C = 0;\n", ""),
    ("state cache reset state: +0xA48 kept", G, "    m_unknownA44[1] = 0;\n", ""),
    ("state cache reset state: +0x9AC kept", G, "    m_unknown9AC = 0;\n", ""),

    # The draw lists.
    ("draw lists constructor: +0x28 left", G, "    m_unknown28 = 0;\n}\n\nAtGfxDrawLists::~AtGfxDrawLists()", "}\n\nAtGfxDrawLists::~AtGfxDrawLists()"),
    ("draw lists destructor: no clear", G, "AtGfxDrawLists::~AtGfxDrawLists() {\n    AtGfxDrawLists::Clear();\n}", "AtGfxDrawLists::~AtGfxDrawLists() {\n}"),
    ("draw lists clear: +0x24 not freed", G, _DL_FREE, _DL_FREE.replace("    if (m_block24 != 0) {\n        operator delete(m_block24);\n    }\n", "")),
    ("draw lists clear: +0x24 and +0x18 in the other order", G, _DL_FREE,
     "    if (m_block18 != 0) {\n        operator delete(m_block18);\n    }\n    if (m_block24 != 0) {\n        operator delete(m_block24);\n    }\n"),
    ("draw lists clear: +0x0C not freed", G, "    if (m_block0C != 0) {\n        operator delete(m_block0C);\n    }\n", ""),
    ("draw lists clear: +0x24 kept", G, _DL_WORDS, _DL_WORDS.replace("    m_block24 = 0;\n", "")),
    ("draw lists clear: +0x04 kept", G, _DL_WORDS, _DL_WORDS.replace("    m_unknown04 = 0;\n", "")),
    ("draw lists clear: +0x1C kept", G, _DL_WORDS, _DL_WORDS.replace("    m_unknown1C = 0;\n", "")),

    # The index array, the buffer set, the mesh database.
    ("index array: resource left", G, "    m_resource = 0;\n    m_memory = 0;\n    m_owner = 0;\n", "    m_memory = 0;\n    m_owner = 0;\n"),
    ("index array: memory left", G, "    m_resource = 0;\n    m_memory = 0;\n    m_owner = 0;\n", "    m_resource = 0;\n    m_owner = 0;\n"),
    ("index array: owner left", G, "    m_resource = 0;\n    m_memory = 0;\n    m_owner = 0;\n", "    m_resource = 0;\n    m_memory = 0;\n"),
    ("buffer set: +0x04 left", G, "AtGfxBufferSet::AtGfxBufferSet() {\n    m_unknown04 = 0;\n", "AtGfxBufferSet::AtGfxBufferSet() {\n"),
    ("buffer set: +0x2C left", G, "    m_unknown2C = 0;\n    m_unknown78", "    m_unknown78"),
    ("buffer set: +0x88 left", G, "    m_unknown88 = 0;\n", ""),
    ("mesh database: meshes left", "AtGrD3DMeshDatabase.cpp", "m_owner(0), m_meshes(0), m_vertexArrays(0)", "m_owner(0), m_vertexArrays(0)"),
    ("mesh database: next left", "AtGrD3DMeshDatabase.cpp", "m_contiguousSize(0), m_next(0) {", "m_contiguousSize(0) {"),
    ("mesh database: contiguous block left", "AtGrD3DMeshDatabase.cpp", "m_keptSize(0), m_contiguous(0), m_contiguousSize(0)", "m_keptSize(0), m_contiguousSize(0)"),

    # The lists of blocks.
    ("block list constructor: +0x10 left", G, "    m_block10 = 0;\n}\n\nvoid AtGfxBlockList::Clear()", "}\n\nvoid AtGfxBlockList::Clear()"),
    ("block list clear: +0x10 not freed", G, "    if (m_block10 != 0) {\n        operator delete(m_block10);\n    }\n", ""),
    ("block list clear: +0x08 and +0x00 in the other order", G, _BL_FREE,
     _BL_FREE.replace("    if (m_block08 != 0) {\n        operator delete(m_block08);\n    }\n    if (m_block00 != 0) {\n        operator delete(m_block00);\n    }\n",
                      "    if (m_block00 != 0) {\n        operator delete(m_block00);\n    }\n    if (m_block08 != 0) {\n        operator delete(m_block08);\n    }\n")),
    ("block list clear: +0x0C kept", G, _BL_FREE, _BL_FREE.replace("    m_unknown0C = 0;\n", "")),
    ("indexed list constructor: +0x20 left", G, "    m_unknown20 = 0;\n}\n\nAtGfxIndexedList::~AtGfxIndexedList()", "}\n\nAtGfxIndexedList::~AtGfxIndexedList()"),
    ("indexed list destructor: no clear", G, "AtGfxIndexedList::~AtGfxIndexedList() {\n    Clear();\n}", "AtGfxIndexedList::~AtGfxIndexedList() {\n}"),
    ("indexed list clear: base not cleared", G, "    AtGfxBlockList::Clear();\n    if (m_block1C", "    if (m_block1C"),
    ("indexed list clear: +0x1C and +0x14 in the other order", G, _IL_FREE,
     "    if (m_block14 != 0) {\n        operator delete(m_block14);\n    }\n    if (m_block1C != 0) {\n        operator delete(m_block1C);\n    }\n"),
    ("indexed list clear: +0x18 kept", G, "    m_unknown18 = 0;\n    m_block1C = 0;\n    m_unknown20 = 0;\n}\n\nAtGfxBlockPair", "    m_block1C = 0;\n    m_unknown20 = 0;\n}\n\nAtGfxBlockPair"),
    ("block pair constructor: +0x0C left", G, "    m_unknown0C = 0;\n}\n\nvoid AtGfxBlockPair::Clear()", "}\n\nvoid AtGfxBlockPair::Clear()"),
    ("block pair clear: +0x08 not freed", G, _PAIR_FREE, _PAIR_FREE.replace("    if (m_block08 != 0) {\n        operator delete(m_block08);\n    }\n", "")),
    ("block pair clear: +0x04 kept", G, _PAIR_FREE, _PAIR_FREE.replace("    m_unknown04 = 0;\n", "")),

    # The resources.
    ("resource: +0x10 left", G, "    m_unknown04[3] = 0;\n", ""),
    ("resource: 0 at +0x2E", G, "    m_unknown2E = 1;\n", "    m_unknown2E = 0;\n"),
    ("resource: +0x30 left", G, "    m_unknown30 = 0;  // The original stores it twice.\n", ""),
    ("resource: +0x26 left", G, "    m_unknown26 = 0;\n", ""),
    ("resource block: +0x34 left", G, "    m_unknown34 = 0;\n    m_unknown38 = 0;\n    m_unknown3C = 0;\n", "    m_unknown38 = 0;\n    m_unknown3C = 0;\n"),
    ("resource block: +0x3C left", G, "    m_unknown34 = 0;\n    m_unknown38 = 0;\n    m_unknown3C = 0;\n", "    m_unknown34 = 0;\n    m_unknown38 = 0;\n"),
    ("resource: +0x48 left", G, "    m_unknown44 = 0;\n    m_resource = 0;\n", "    m_unknown44 = 0;\n"),
    ("resource: +0x40 left", G, "    m_unknown40 = 0;\n    m_unknown44 = 0;\n", "    m_unknown44 = 0;\n"),

    # The ordering.
    ("less: or equal", G, "return ((const float*)a)[4] < ((const float*)b)[4];", "return ((const float*)a)[4] <= ((const float*)b)[4];"),
    ("less: +0x0C", G, "return ((const float*)a)[4] < ((const float*)b)[4];", "return ((const float*)a)[3] < ((const float*)b)[4];"),

    # The device: constructor and Init.
    ("device constructor: creator left", G, "    m_creator = 0;\n    Init();\n", "    Init();\n"),
    ("device constructor: no init", G, "    m_creator = 0;\n    Init();\n", "    m_creator = 0;\n"),
    ("init: +0x3AC kept", G, "    m_unknown3AC = 0;\n", ""),
    ("init: +0x332C kept", G, "    m_unknown332C = 0;\n", ""),
    ("init: contexts kept", G, "    m_contexts = 0;\n", ""),
    ("init: mesh databases kept", G, "    m_meshDatabases = 0;\n", ""),
    ("init: +0x3350 kept", G, "    m_unknown3350 = 0;\n", ""),
    ("init: +0x3F7C short",) + (G,) + _loop("m_unknown3F7C", "10", "9"),
    ("init: +0x3FA4 at the next table", G, "    m_unknown3FA4 = m_table3FA8;\n", "    m_unknown3FA4 = m_table43A8;\n"),
    ("init: table 3FA8 short",) + (G,) + _loop("m_table3FA8", "0x100", "0xFF"),
    ("init: table 47B4 short",) + (G,) + _loop("m_table47B4", "0x100", "0xFF"),
    ("init: table 5BE4 short",) + (G,) + _loop("m_table5BE4", "0x100", "0xFF"),
    ("init: +0x5FF0 kept", G, "    m_unknown5FE4[3] = 0;\n", ""),
    ("init: +0x5FF4 kept", G, "    m_unknown5FF4 = 0;\n", ""),
    ("init: +0x6100 kept", G, "    m_unknown6100 = 0;\n", ""),
    ("init: +0x6104 short",) + (G,) + _loop("m_unknown6104", "0x10", "0xF"),
    ("init: +0x6148 kept", G, "    m_unknown6148 = 0;\n", ""),
    ("init: +0x614C short",) + (G,) + _loop("m_unknown614C", "0x3400", "0x33FF"),
    ("init: second ordering not set", G, "    m_sortedB.m_less = AtGfxLessAt10;\n", ""),
    ("init: +0x30240 short",) + (G,) + _loop("m_unknown30240", "5", "4"),
    ("init: 0x900 at +0x3E10", G, "    m_unknown3E10 = 0x901;\n", "    m_unknown3E10 = 0x900;\n"),
    ("init: +0x30254 short",) + (G,) + _loop("m_unknown30254", "0x11", "0x10"),
    ("init: +0x3DF8 kept", G, "    m_unknown3DF8 = 0;\n", ""),
    ("init: 0 at +0x3E00", G, "    m_unknown3E00 = 1;\n", "    m_unknown3E00 = 0;\n"),
    ("init: +0x3E0C kept", G, "    m_unknown3E0C = 0;\n", ""),
    ("init: 1 at +0x30598", G, "    m_unknown30598 = 2;\n", "    m_unknown30598 = 1;\n"),
    ("init: +0x305A0 short",) + (G,) + _loop("m_unknown305A0", "4", "3"),

    # Taking a context out.
    ("unlink: the rest dropped", G, "            *link = context->m_next;\n", "            *link = 0;\n"),
    ("unlink: the first other taken", G, "        if (c == context) {\n", "        if (c != context) {\n"),

    # The slots.
    ("slot 13: the other target", G, "void* AtGraphicsDeviceXbox::RenderTarget0() {\n    return &m_target0;", "void* AtGraphicsDeviceXbox::RenderTarget0() {\n    return &m_target1;"),
    ("slot 14: the other target", G, "void* AtGraphicsDeviceXbox::RenderTarget1() {\n    return &m_target1;", "void* AtGraphicsDeviceXbox::RenderTarget1() {\n    return &m_target0;"),
    ("slot 15: another member", G, "    return &m_context;\n", "    return (AtGfxContext*)&m_textureA2ED0;\n"),
    ("slot 16: another member", G, "    return &m_drawLists;\n", "    return (AtGfxDrawLists*)&m_unknown3E10;\n"),
    ("slot 17: another member", G, "    return &m_matrixDevice;\n", "    return (AtMatrixDevice*)&m_bufferSet;\n"),
    ("slot 18: null", G, "    return static_cast<AtFlashGraphicsFactory*>(m_creator);", "    return 0;"),
    ("slot 20: slot 0", G, "    object->Release();\n", "    object->Slot0();\n"),
    ("slot 21: another member", G, "    return &m_bufferSet;\n", "    return (AtGfxBufferSet*)&m_meshDatabase;\n"),
    ("slot 23: the height", G, "    return m_mode.m_width;\n", "    return m_mode.m_height;\n"),
    ("slot 24: the width", G, "    return m_mode.m_height;\n", "    return m_mode.m_width;\n"),
    ("slot 25: half", G, "    out[1] = 1.0f;\n", "    out[1] = 0.5f;\n"),
    ("slot 25: a third written", G, "    out[1] = 1.0f;\n", "    out[1] = 1.0f;\n    out[2] = 1.0f;\n"),
]

SUITE = {"target": "oracle_gfxdev", "subsystem": "at/gfxdev", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
