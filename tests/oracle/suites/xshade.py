# Mutants of the Xbox shader, light, font and image databases and what they hold. Each text occurs exactly once in its file.
S = "AtGrD3DShaderDatabase.cpp"
F = "AtGrD3DFontImage.cpp"

# A light's constructor, destructor and Reset store the same nineteen values; each is mutated in each, the text running from the function's head to the line.
LIGHT_LINES = [
    ("owner", "    m_owner = 0;\n", ""),
    ("ambient x", "    m_ambient.x = 1.0f;\n", "    m_ambient.x = 0.5f;\n"),
    ("ambient y", "    m_ambient.y = 1.0f;\n", "    m_ambient.y = 0.5f;\n"),
    ("ambient z", "    m_ambient.z = 1.0f;\n", "    m_ambient.z = 0.5f;\n"),
    ("ambient w", "    m_ambient.w = 1.0f;\n", "    m_ambient.w = 0.5f;\n"),
    ("diffuse x", "    m_diffuse.x = 1.0f;\n", "    m_diffuse.x = 0.5f;\n"),
    ("diffuse y", "    m_diffuse.y = 1.0f;\n", "    m_diffuse.y = 0.5f;\n"),
    ("diffuse z", "    m_diffuse.z = 1.0f;\n", "    m_diffuse.z = 0.5f;\n"),
    ("diffuse w", "    m_diffuse.w = 1.0f;\n", "    m_diffuse.w = 0.5f;\n"),
    ("specular x", "    m_specular.x = 1.0f;\n", "    m_specular.x = 0.5f;\n"),
    ("specular y", "    m_specular.y = 1.0f;\n", "    m_specular.y = 0.5f;\n"),
    ("specular z", "    m_specular.z = 1.0f;\n", "    m_specular.z = 0.5f;\n"),
    ("specular w", "    m_specular.w = 1.0f;\n", "    m_specular.w = 0.5f;\n"),
    ("inner cone", "    m_innerConeAngle = 1.0f;\n", "    m_innerConeAngle = 0.5f;\n"),
    ("outer cone", "    m_outerConeAngle = 1.0f;\n", "    m_outerConeAngle = 0.5f;\n"),
    ("range", "    m_range = 1000.0f;\n", "    m_range = 999.0f;\n"),
    ("attenuation 0", "    m_attenuation[0] = 1.0f;\n", "    m_attenuation[0] = 0.5f;\n"),
    ("attenuation 1", "    m_attenuation[1] = 0.0f;\n", "    m_attenuation[1] = 2.0f;\n"),
    ("attenuation 2", "    m_attenuation[2] = 0.0f;\n", "    m_attenuation[2] = 2.0f;\n"),
]

def light_mutants():
    out = []
    for fn, head in (("light ctor", "AtLight::AtLight() {\n"), ("light dtor", "AtLight::~AtLight() {\n"), ("light reset", "void AtLight::Reset() {\n")):
        before = ""
        for name, line, new in LIGHT_LINES:
            out.append(("%s: %s" % (fn, name), S, head + before + line, head + before + new))
            before += line
    return out

MUTANTS = [
    # Shaders.
    ("shader db ctor: array", S, "AtGrD3DShaderDatabase::AtGrD3DShaderDatabase() {\n    m_shaders = 0;\n}", "AtGrD3DShaderDatabase::AtGrD3DShaderDatabase() {\n}"),
    ("shader db dtor: not cleared", S, "AtGrD3DShaderDatabase::~AtGrD3DShaderDatabase() {\n    AtGrD3DShaderDatabase::Clear();\n}", "AtGrD3DShaderDatabase::~AtGrD3DShaderDatabase() {\n}"),
    ("shader clear: array kept", S, "    if (m_shaders != 0) {\n        delete[] m_shaders;\n    }\n", ""),
    ("shader clear: base kept", S, "    AtShaderDatabase::Clear();\n    m_shaders = 0;", "    m_shaders = 0;"),
    ("shader clear: pointer kept", S, "    AtShaderDatabase::Clear();\n    m_shaders = 0;\n", "    AtShaderDatabase::Clear();\n"),
    ("shader make: no raise without heap", S, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_shaders", "    AtPushHeap(s_heap);\n    m_shaders"),
    ("shader make: heap not pushed", S, "    AtPushHeap(s_heap);\n    m_shaders = new", "    m_shaders = new"),
    ("shader make: one too many", S, "new AtGrD3DShader[m_count];", "new AtGrD3DShader[m_count + 1];"),
    ("shader make: heap not popped", S, "    m_shaders = new AtGrD3DShader[m_count];\n    AtPopHeap();\n", "    m_shaders = new AtGrD3DShader[m_count];\n"),
    ("shader make: no raise on no memory", S, "    if (m_shaders == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("shader at: next", S, "    return &m_shaders[index];", "    return &m_shaders[index + 1];"),
    # Lights.
    ("light heap: not kept", S, "void AtLightDatabase::SetHeap(void* heap) {\n    s_heap = heap;", "void AtLightDatabase::SetHeap(void* heap) {\n    s_heap = 0;"),
    ("light db ctor: array", S, "AtGrD3DLightDatabase::AtGrD3DLightDatabase() {\n    m_lights = 0;\n", "AtGrD3DLightDatabase::AtGrD3DLightDatabase() {\n"),
    ("light db ctor: owner", S, "    m_lights = 0;\n    m_owner = 0;\n}\n\nAtGrD3DLightDatabase::~", "    m_lights = 0;\n}\n\nAtGrD3DLightDatabase::~"),
    ("light db dtor: not cleared", S, "AtGrD3DLightDatabase::~AtGrD3DLightDatabase() {\n    AtGrD3DLightDatabase::Clear();\n}", "AtGrD3DLightDatabase::~AtGrD3DLightDatabase() {\n}"),
    ("light clear: array kept", S, "    if (m_lights != 0) {\n        delete[] m_lights;\n    }\n", ""),
    ("light clear: pointer kept", S, "        delete[] m_lights;\n    }\n    m_lights = 0;\n", "        delete[] m_lights;\n    }\n"),
    ("light clear: owner kept", S, "    m_lights = 0;\n    m_owner = 0;\n    AtLightDatabase::Clear();", "    m_lights = 0;\n    AtLightDatabase::Clear();"),
    ("light clear: base kept", S, "    m_owner = 0;\n    AtLightDatabase::Clear();\n", "    m_owner = 0;\n"),
    ("light make: no raise without heap", S, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_lights", "    AtPushHeap(s_heap);\n    m_lights"),
    ("light make: heap not pushed", S, "    AtPushHeap(s_heap);\n    m_lights = new", "    m_lights = new"),
    ("light make: one too many", S, "new AtGrD3DLight[m_count];", "new AtGrD3DLight[m_count + 1];"),
    ("light make: heap not popped", S, "    m_lights = new AtGrD3DLight[m_count];\n    AtPopHeap();\n", "    m_lights = new AtGrD3DLight[m_count];\n"),
    ("light make: no raise on no memory", S, "    if (m_lights == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("light make: owner not kept", S, "    m_owner = owner;\n", ""),
    ("light make: owner only with memory", S, "        AtRaise(-4, 0, 0, 0);\n    }\n    m_owner = owner;", "        AtRaise(-4, 0, 0, 0);\n        return;\n    }\n    m_owner = owner;"),
    ("light at: next", S, "    return &m_lights[index];", "    return &m_lights[index + 1];"),
] + light_mutants() + [
    # The owner's lists.
    ("font list: add not linked", F, "    database->m_next = m_fontDatabases;\n", ""),
    ("font list: add not at head", F, "    m_fontDatabases = database;\n}", "}"),
    ("font list: remove relinks wrong", F, "        if (at == database) {\n            *link = database->m_next;\n            return;\n        }\n        link = &at->m_next;\n    }\n}\n\nvoid AtGrD3DOwner::AddImageDatabase", "        if (at == database) {\n            *link = 0;\n            return;\n        }\n        link = &at->m_next;\n    }\n}\n\nvoid AtGrD3DOwner::AddImageDatabase"),
    ("font list: remove stops at first", F, "        link = &at->m_next;\n    }\n}\n\nvoid AtGrD3DOwner::AddImageDatabase", "        return;\n    }\n}\n\nvoid AtGrD3DOwner::AddImageDatabase"),
    ("image list: add not linked", F, "    database->m_next = m_imageDatabases;\n", ""),
    ("image list: add not at head", F, "    m_imageDatabases = database;\n}", "}"),
    ("image list: remove relinks wrong", F, "        if (at == database) {\n            *link = database->m_next;\n            return;\n        }\n        link = &at->m_next;\n    }\n}\n\n// ---- 2D fonts", "        if (at == database) {\n            *link = 0;\n            return;\n        }\n        link = &at->m_next;\n    }\n}\n\n// ---- 2D fonts"),
    ("image list: remove stops at first", F, "        link = &at->m_next;\n    }\n}\n\n// ---- 2D fonts", "        return;\n    }\n}\n\n// ---- 2D fonts"),
    # 2D fonts.
    ("font dtor: not freed", F, "AtGrD3DFont2D::~AtGrD3DFont2D() {\n    AtGrD3DFont2D::Free();\n}", "AtGrD3DFont2D::~AtGrD3DFont2D() {\n}"),
    ("font free: surface kept", F, "void AtGrD3DFont2D::Free() {\n    m_surface.Reset();\n", "void AtGrD3DFont2D::Free() {\n"),
    ("font free: textures kept", F, "    m_surface.Reset();\n    m_textures.Clear();\n    if (m_flags & kBorrowedChars) {", "    m_surface.Reset();\n    if (m_flags & kBorrowedChars) {"),
    ("font free: always borrowed", F, "    if (m_flags & kBorrowedChars) {", "    if (true) {"),
    ("font free: never forgotten", F, "        m_chars = 0;\n    }\n    AtGrI2DFont::Free();", "    }\n    AtGrI2DFont::Free();"),
    ("font free: wrong flag", F, "    if (m_flags & kBorrowedChars) {", "    if (m_flags & 0x100) {"),
    ("font free: base kept", F, "    AtGrI2DFont::Free();\n}", "}"),
    ("font loaded: not the seam", F, "    AtGrD3DLoadFont2D(this, owner);\n", ""),
    ("font free data: always", F, "void AtGrD3DFont2D::FreeData() {\n    if (m_flags & kOwnsData) {", "void AtGrD3DFont2D::FreeData() {\n    if (true) {"),
    ("font free data: surface kept", F, "void AtGrD3DFont2D::FreeData() {\n    if (m_flags & kOwnsData) {\n        m_surface.Reset();\n", "void AtGrD3DFont2D::FreeData() {\n    if (m_flags & kOwnsData) {\n"),
    ("font free data: textures kept", F, "        m_surface.Reset();\n        m_textures.Clear();\n        AtGrI2DFont::FreeData();", "        m_surface.Reset();\n        AtGrI2DFont::FreeData();"),
    ("font free data: base kept", F, "        AtGrI2DFont::FreeData();\n", ""),
    # The font database.
    ("font db ctor: owner", F, "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n    m_owner = 0;\n", "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n"),
    ("font db ctor: fonts", F, "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n    m_owner = 0;\n    m_fonts = 0;\n", "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n    m_owner = 0;\n"),
    ("font db ctor: next", F, "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n    m_owner = 0;\n    m_fonts = 0;\n    m_next = 0;\n", "AtGrD3DFontDatabase::AtGrD3DFontDatabase() {\n    m_owner = 0;\n    m_fonts = 0;\n"),
    ("font db ctor: binary", F, "    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n}\n\nAtGrD3DFontDatabase::~", "    m_next = 0;\n    m_unknown34 = 0;\n}\n\nAtGrD3DFontDatabase::~"),
    ("font db ctor: +34", F, "    m_binary = 0;\n    m_unknown34 = 0;\n}\n\nAtGrD3DFontDatabase::~", "    m_binary = 0;\n}\n\nAtGrD3DFontDatabase::~"),
    ("font db dtor: not cleared", F, "AtGrD3DFontDatabase::~AtGrD3DFontDatabase() {\n    AtGrD3DFontDatabase::Clear();\n}", "AtGrD3DFontDatabase::~AtGrD3DFontDatabase() {\n}"),
    ("font clear: binary kept", F, "    if (m_binary != 0) {\n        operator delete(m_binary);\n    }\n", ""),
    ("font clear: not off the list", F, "    if (m_owner != 0) {\n        m_owner->RemoveFontDatabase(this);\n    }\n", ""),
    ("font clear: off the list without an owner", F, "    if (m_owner != 0) {\n        m_owner->RemoveFontDatabase(this);", "    if (true) {\n        m_owner->RemoveFontDatabase(this);"),
    ("font clear: array kept", F, "    if (m_fonts != 0) {\n        delete[] m_fonts;\n    }\n", ""),
    ("font clear: owner kept", F, "    m_owner = 0;\n    m_fonts = 0;\n    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();", "    m_fonts = 0;\n    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();"),
    ("font clear: fonts kept", F, "    m_fonts = 0;\n    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();", "    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();"),
    ("font clear: next kept", F, "    m_next = 0;\n    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();", "    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();"),
    ("font clear: binary pointer kept", F, "    m_binary = 0;\n    m_unknown34 = 0;\n    AtFontDatabase::Clear();", "    m_unknown34 = 0;\n    AtFontDatabase::Clear();"),
    ("font clear: +34 kept", F, "    m_unknown34 = 0;\n    AtFontDatabase::Clear();", "    AtFontDatabase::Clear();"),
    ("font clear: base kept", F, "    m_unknown34 = 0;\n    AtFontDatabase::Clear();\n", "    m_unknown34 = 0;\n"),
    ("font ready: owner not kept", F, "    m_owner = static_cast<AtGrD3DOwner*>(owner);\n    m_owner->AddFontDatabase(this);", "    static_cast<AtGrD3DOwner*>(owner)->AddFontDatabase(this);"),
    ("font ready: not on the list", F, "    m_owner->AddFontDatabase(this);\n", ""),
    ("font make: no raise without heap", F, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_fonts", "    AtPushHeap(s_heap);\n    m_fonts"),
    ("font make: heap not pushed", F, "    AtPushHeap(s_heap);\n    m_fonts = new", "    m_fonts = new"),
    ("font make: one too many", F, "new AtGrD3DFont2D[m_count];", "new AtGrD3DFont2D[m_count + 1];"),
    ("font make: heap not popped", F, "    m_fonts = new AtGrD3DFont2D[m_count];\n    AtPopHeap();\n", "    m_fonts = new AtGrD3DFont2D[m_count];\n"),
    ("font make: no raise on no memory", F, "    if (m_fonts == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("font binary form: none", F, "int AtGrD3DFontDatabase::Slot4() {\n    return 1;", "int AtGrD3DFontDatabase::Slot4() {\n    return 0;"),
    ("font at: next", F, "    return &m_fonts[index];", "    return &m_fonts[index + 1];"),
    # Images.
    # Dropped, provably equivalent: "image dtor: not freed". Without its Free, the members' destructors that follow reset the surface (0x000FD8C0 begins with the same Reset) and clear the texture database
    # (0x001101D0 begins with the same Clear), in the same order, the surface being declared after the textures; then AtImage's destructor (0x00058650) empties the image as AtImage::Free does. The same
    # work in the same order, on an object about to be freed. (The font's is not equivalent: there AtGrI2DFont's destructor frees a borrowed table that the font's Free forgets; that mutant is detected.)
    ("image free: surface kept", F, "void AtGrD3DImage::Free() {\n    m_surface.Reset();\n", "void AtGrD3DImage::Free() {\n"),
    ("image free: textures kept", F, "    m_surface.Reset();\n    m_textures.Clear();\n    AtImage::Free();", "    m_surface.Reset();\n    AtImage::Free();"),
    ("image free: base kept", F, "    m_textures.Clear();\n    AtImage::Free();\n", "    m_textures.Clear();\n"),
    ("image loaded: not the seam", F, "    AtGrD3DLoadImage(this, owner);\n", ""),
    ("image free data: always", F, "void AtGrD3DImage::FreeData() {\n    if (m_flags & kOwnsData) {", "void AtGrD3DImage::FreeData() {\n    if (true) {"),
    ("image free data: textures kept", F, "void AtGrD3DImage::FreeData() {\n    if (m_flags & kOwnsData) {\n        m_textures.Clear();\n", "void AtGrD3DImage::FreeData() {\n    if (m_flags & kOwnsData) {\n"),
    ("image free data: surface kept", F, "        m_textures.Clear();\n        m_surface.Reset();\n        AtImage::FreeData();", "        m_textures.Clear();\n        AtImage::FreeData();"),
    ("image free data: base kept", F, "        AtImage::FreeData();\n", ""),
    # The image database.
    ("image db ctor: owner", F, "AtGrD3DImageDatabase::AtGrD3DImageDatabase() {\n    m_owner = 0;\n", "AtGrD3DImageDatabase::AtGrD3DImageDatabase() {\n"),
    ("image db ctor: images", F, "AtGrD3DImageDatabase::AtGrD3DImageDatabase() {\n    m_owner = 0;\n    m_images = 0;\n", "AtGrD3DImageDatabase::AtGrD3DImageDatabase() {\n    m_owner = 0;\n"),
    ("image db ctor: next", F, "    m_images = 0;\n    m_next = 0;\n}\n\nAtGrD3DImageDatabase::~", "    m_images = 0;\n}\n\nAtGrD3DImageDatabase::~"),
    ("image db dtor: not cleared", F, "AtGrD3DImageDatabase::~AtGrD3DImageDatabase() {\n    AtGrD3DImageDatabase::Clear();\n}", "AtGrD3DImageDatabase::~AtGrD3DImageDatabase() {\n}"),
    ("image clear: array kept", F, "    if (m_images != 0) {\n        delete[] m_images;\n    }\n", ""),
    ("image clear: not off the list", F, "    if (m_owner != 0) {\n        m_owner->RemoveImageDatabase(this);\n    }\n", ""),
    ("image clear: off the list without an owner", F, "    if (m_owner != 0) {\n        m_owner->RemoveImageDatabase(this);", "    if (true) {\n        m_owner->RemoveImageDatabase(this);"),
    ("image clear: owner kept", F, "    m_owner = 0;\n    m_images = 0;\n    m_next = 0;\n    AtImageDatabase::Clear();", "    m_images = 0;\n    m_next = 0;\n    AtImageDatabase::Clear();"),
    ("image clear: images kept", F, "    m_images = 0;\n    m_next = 0;\n    AtImageDatabase::Clear();", "    m_next = 0;\n    AtImageDatabase::Clear();"),
    ("image clear: next kept", F, "    m_next = 0;\n    AtImageDatabase::Clear();", "    AtImageDatabase::Clear();"),
    ("image clear: base kept", F, "    m_next = 0;\n    AtImageDatabase::Clear();\n", "    m_next = 0;\n"),
    ("image make: no raise without heap", F, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_images", "    AtPushHeap(s_heap);\n    m_images"),
    ("image make: heap not pushed", F, "    AtPushHeap(s_heap);\n    m_images = new", "    m_images = new"),
    ("image make: one too many", F, "new AtGrD3DImage[m_count];", "new AtGrD3DImage[m_count + 1];"),
    ("image make: heap not popped", F, "    m_images = new AtGrD3DImage[m_count];\n    AtPopHeap();\n", "    m_images = new AtGrD3DImage[m_count];\n"),
    ("image make: no raise on no memory", F, "    if (m_images == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("image ready: owner not kept", F, "    m_owner = static_cast<AtGrD3DOwner*>(owner);\n    m_owner->AddImageDatabase(this);", "    static_cast<AtGrD3DOwner*>(owner)->AddImageDatabase(this);"),
    ("image ready: not on the list", F, "    m_owner->AddImageDatabase(this);\n", ""),
    ("image at: next", F, "    return &m_images[index];", "    return &m_images[index + 1];"),
]

SUITE = {"target": "oracle_xshade", "subsystem": "at/xshade", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
