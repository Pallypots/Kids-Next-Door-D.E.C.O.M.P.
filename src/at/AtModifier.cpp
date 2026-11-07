// Modifiers and the .ASM format. See include/at/AtModifier.h.
#include "at/AtModifier.h"

#include "at/AtText.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtModifier::s_heap;
void* AtGroupModifier::s_heap;
void* AtModifierDatabase::s_heap;

const AtIoKeyword AtModifierDatabase::s_keywords[AtModifierDatabase::kKeywordCount] = {
    {57, "Modifier"}, {58, "LightModifier"}, {59, "Light"}, {60, "Directional"}, {61, "Point"}, {62, "SpotLight"}, {63, "Position"}, {64, "Direction"},
    {65, "AmbientModifier"}, {66, "TextureProjectorModifier"}, {67, "Window"}, {68, "Fov"}, {69, "AspectRatio"}, {70, "NearDepth"}, {71, "FarDepth"}, {72, "MaterialMask"},
    {73, "MatAssignment"}, {74, "TextureTransformModifier"}, {75, "Rotate2D"}, {76, "Translate2D"}, {77, "Scale2D"}, {78, "Rotate3D"}, {79, "Translate3D"}, {80, "Scale3D"},
    {81, "Shader"}, {82, "ProgShaderStage"}, {83, "StagedShaderStage"}, {84, "ProjectedShadowModifier"}, {85, "PlaneShadowModifier"}, {86, "Plane"}, {87, "Color"}, {88, "CubeMapModifier"},
    {89, "VertexColorTransformModifier"}, {90, "LeftShift"}, {91, "RightShift"}, {92, "Add"}, {93, "Subtract"}, {94, "Modulate"}, {95, "MaterialOverrideModifier"}, {96, "Material"},
    {97, "AmbientColor"}, {98, "DiffuseColor"}, {99, "SpecularColor"}, {100, "ConstantColor"}, {101, "ConstantAlpha"}, {102, "GroupModifier"}, {103, "ReflectionGroupModifier"}, {104, "ReflectionPlane"},
    {105, "BlockModifier"}, {106, "Block"}, {107, "Priority"}, {108, "NameSpace"}, {109, "FieldOfViewY"}, {110, "PerspectiveModifier"}, {111, "ContextConstantModifier"}, {112, "VertexProcessConstant"},
    {113, "PixelProcessConstant"}, {114, "LightAnimation"}};

namespace {

void NotRecovered() {
    AtFatal("Atlas Modifier: this slot belongs to drawing and is not recovered", 0, 0, 0);
}

// A list with no constructors, from one class's heap.
void* NewBlock(void* heap, unsigned int size) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    void* block = operator new(size);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}

void ReadConstants(AtIoTokenizer* tokenizer, unsigned int* count, AtContextConstantModifier::Constant** list) {
    if (*list != 0) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    *count = (unsigned int)tokenizer->ExpectCount();
    if (*count < 1) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    *list = (AtContextConstantModifier::Constant*)NewBlock(AtModifier::s_heap, *count * sizeof(AtContextConstantModifier::Constant));
    for (unsigned int i = 0; i < *count; i++) {
        AtContextConstantModifier::Constant& constant = (*list)[i];
        constant.m_value[0] = tokenizer->ExpectReal();
        constant.m_value[1] = tokenizer->ExpectReal();
        constant.m_value[2] = tokenizer->ExpectReal();
        constant.m_value[3] = tokenizer->ExpectReal();
        constant.m_index = tokenizer->ExpectInt();
    }
    tokenizer->ExpectRightCurly();
}

}  // namespace

// ---- the base ------------------------------------------------------------------------------------------------------

void AtModifier::SetHeap(void* heap) {
    s_heap = heap;
}

AtModifier::AtModifier() {
    m_kind = -1;
    m_priority = 0;
    m_flags = 0;
}

AtModifier::~AtModifier() {}

void AtModifier::Reset() {
    m_kind = -1;
    m_priority = 0;
    m_flags = 0;
}

void AtModifier::ReadWord(AtIoTokenizer* tokenizer, int word) {
    if (word == kAtWordPriority) {
        m_priority = tokenizer->ExpectInt();
    } else {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
}

void AtModifier::Apply(AtModifierState* state, int) {
    if ((m_flags & kDisabled) == 0) {
        state->Set(this);
    }
}

void AtModifier::Slot2(void*, void*, void*, void*, void*) {}
void AtModifier::Slot3(void*, void*, void*, void*) {}
void AtModifier::Slot4(void*) {}
void AtModifier::Slot5(void*) {}

// ---- lights ------------------------------------------------------------------------------------------------------

AtLightModifier::AtLightModifier() {
    m_light = 0;
    m_unknown30 = 0;
    m_type = kDirectional;
    m_direction.y = 0;
    m_direction.z = 0;
    m_position.x = 0;
    m_position.y = 0;
    m_position.z = 0;
    m_direction.x = 1.0f;
}

AtLightModifier::~AtLightModifier() {}

void AtLightModifier::Read(AtSceneDevice* device, AtIoTokenizer* tokenizer) {
    m_kind = kAtModifierLight;
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        switch (word) {
        case kAtWordLight: {
            char name[8];
            char ns[8];
            tokenizer->ExpectName(name);
            tokenizer->ExpectName(ns);
            m_light = ns[0] != 0 ? device->m_lightSpaces.Find(ns, name) : device->m_lights.Find(name);
            if (m_light == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            break;
        }
        case kAtWordLightAnimation: {
            char name[8];
            char ns[8];
            tokenizer->ExpectName(name);
            tokenizer->ExpectName(ns);
            void* animation = ns[0] != 0 ? device->m_lightAnimationSpaces.Find(ns, name) : device->m_lightAnimations.Find(name);
            if (animation == 0) {
                tokenizer->Error(kAtProblemNotFound);
            } else {
                m_playback.SetAnimation((AtSceneMatAnimation*)animation);
                m_playback.m_flags |= AtScenePlayback::kStarted;
                if (m_playback.m_listener != 0) {
                    m_playback.m_listener->OnStarted(&m_playback);
                }
                m_playback.m_flags |= AtScenePlayback::kPlaying;
            }
            break;
        }
        case kAtWordDirectional:
            m_type = kDirectional;
            break;
        case kAtWordPoint:
            m_type = kPoint;
            break;
        case kAtWordSpotLight:
            m_type = kSpot;
            break;
        case kAtWordLightPosition:
            m_position.Read(tokenizer);
            break;
        case kAtWordDirection:
            m_direction.Read(tokenizer);
            m_direction.Normalize();
            break;
        default:
            ReadWord(tokenizer, word);
            break;
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtLightModifier::Apply(AtModifierState* state, int data) {
    if ((m_flags & kDisabled) == 0 && (unsigned int)m_kind < state->m_count) {
        state->m_slots[m_kind].Insert(this, data);
    }
}

void AtLightModifier::Slot2(void*, void*, void*, void*, void*) { NotRecovered(); }
void AtLightModifier::Slot3(void*, void*, void*, void*) { NotRecovered(); }
void AtLightModifier::Slot4(void*) { NotRecovered(); }
void AtLightModifier::Slot5(void*) { NotRecovered(); }

// ---- ambient ------------------------------------------------------------------------------------------------------

AtAmbientModifier::AtAmbientModifier() {
    m_color.x = 0;
    m_color.y = 0;
    m_color.z = 0;
    m_color.w = 1.0f;
}

AtAmbientModifier::~AtAmbientModifier() {}

void AtAmbientModifier::Read(AtIoTokenizer* tokenizer) {
    m_kind = kAtModifierAmbient;
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        if (word == kAtWordColor) {
            m_color.Read(tokenizer);
        } else {
            ReadWord(tokenizer, word);
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtAmbientModifier::Slot2(void*, void*, void*, void*, void*) { NotRecovered(); }

// ---- perspective ------------------------------------------------------------------------------------------------------

AtPerspectiveModifier::AtPerspectiveModifier() {
    m_fieldOfViewY = 4.0f / 3.0f;
    m_saved = 4.0f / 3.0f;
    m_kind = kAtModifierPerspective;
}

AtPerspectiveModifier::~AtPerspectiveModifier() {}

void AtPerspectiveModifier::Read(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        if (word == kAtWordFieldOfViewY) {
            m_fieldOfViewY = tokenizer->ExpectReal();
        } else {
            ReadWord(tokenizer, word);
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtPerspectiveModifier::Slot2(void*, void*, void*, void*, void*) { NotRecovered(); }
void AtPerspectiveModifier::Slot3(void*, void*, void*, void*) { NotRecovered(); }

// ---- context constants ------------------------------------------------------------------------------------------------------

AtContextConstantModifier::AtContextConstantModifier() {
    m_vertexCount = 0;
    m_vertex = 0;
    m_pixelCount = 0;
    m_pixel = 0;
    m_kind = kAtModifierContextConstant;
}

AtContextConstantModifier::~AtContextConstantModifier() {
    Reset();
    if (m_vertex != 0) {
        operator delete(m_vertex);
    }
    if (m_pixel != 0) {
        operator delete(m_pixel);
    }
    m_vertexCount = 0;
    m_vertex = 0;
    m_pixelCount = 0;
    m_pixel = 0;
}

void AtContextConstantModifier::Read(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        if (word == kAtWordPixelProcessConstant) {
            ReadConstants(tokenizer, &m_pixelCount, &m_pixel);
        } else if (word == kAtWordVertexProcessConstant) {
            ReadConstants(tokenizer, &m_vertexCount, &m_vertex);
        } else {
            ReadWord(tokenizer, word);
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtContextConstantModifier::Slot2(void*, void*, void*, void*, void*) { NotRecovered(); }

// ---- groups ------------------------------------------------------------------------------------------------------

void AtGroupModifier::SetHeap(void* heap) {
    s_heap = heap;
}

AtGroupModifier::AtGroupModifier() {
    m_count = 0;
    m_capacity = 0;
    m_members = 0;
}

AtGroupModifier::~AtGroupModifier() {
    Reset();
    if (m_members != 0) {
        operator delete(m_members);
    }
    m_count = 0;
    m_capacity = 0;
    m_members = 0;
}

void AtGroupModifier::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_members != 0) {
        Reset();
        operator delete(m_members);
        m_count = 0;
        m_capacity = 0;
        m_members = 0;
    }
    m_kind = kAtModifierGroup;
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        if (word == kAtWordModifierList) {
            m_capacity = (unsigned int)tokenizer->ExpectCount();
            m_members = (AtModifier**)NewBlock(s_heap, m_capacity * sizeof(AtModifier*));
            for (unsigned int i = 0; i < m_capacity; i++) {
                char name[8];
                char ns[8];
                tokenizer->ExpectName(name);
                tokenizer->ExpectName(ns);
                AtModifier* member = (AtModifier*)(ns[0] != 0 ? owner->m_modifierSpaces.Find(ns, name) : owner->m_modifiers.Find(name));
                if (member == 0) {
                    tokenizer->Error(kAtProblemNotFound);
                }
                // m_count is not reset by a second list here, so that one fills only what is left of a list that size.
                if (m_count < m_capacity) {
                    m_members[m_count] = member;
                    m_count++;
                }
            }
            tokenizer->ExpectRightCurly();
        } else {
            ReadWord(tokenizer, word);
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtGroupModifier::Apply(AtModifierState* state, int data) {
    if ((m_flags & kDisabled) == 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_members[i]->Apply(state, data);
        }
    }
}

void AtGroupModifier::Slot2(void* a, void* b, void* c, void* d, void* e) {
    for (unsigned int i = 0; i < m_count; i++) {
        m_members[i]->Slot2(a, b, c, d, e);
    }
}

void AtGroupModifier::Slot3(void* a, void* b, void* c, void* d) {
    for (unsigned int i = 0; i < m_count; i++) {
        m_members[i]->Slot3(a, b, c, d);
    }
}

void AtGroupModifier::Slot5(void* a) {
    for (unsigned int i = 0; i < m_count; i++) {
        m_members[i]->Slot5(a);
    }
}

AtModifier* AtGroupModifier::MemberAt(unsigned int index) const {
    return m_count != 0 ? m_members[index] : 0;
}

// ---- blocks ------------------------------------------------------------------------------------------------------

AtBlockModifier::AtBlockModifier() {
    m_blocks = -1;
}

AtBlockModifier::~AtBlockModifier() {}

void AtBlockModifier::Read(AtIoTokenizer* tokenizer) {
    m_kind = kAtModifierBlock;
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        if (word == kAtWordBlock) {
            switch (tokenizer->Next()) {
            case kAtWordLightModifier:
                m_blocks = kAtModifierLight;
                break;
            case kAtWordAmbientModifier:
                m_blocks = kAtModifierAmbient;
                break;
            case kAtWordPerspectiveModifier:
                m_blocks = kAtModifierPerspective;
                break;
            case kAtWordContextConstantModifier:
                m_blocks = kAtModifierContextConstant;
                break;
            default:
                break;
            }
        } else {
            ReadWord(tokenizer, word);
        }
        word = tokenizer->Next();
    }
    m_flags |= kLoaded;
}

void AtBlockModifier::Apply(AtModifierState* state, int) {
    if ((m_flags & kDisabled) == 0 && (unsigned int)m_blocks < state->m_count) {
        new (&state->m_slots[m_blocks], AtPlacement()) AtModifierSlot;
    }
}

// ---- the file ------------------------------------------------------------------------------------------------------

namespace {

// The array of one class. A list word given twice reports, then makes a new array and leaks the first, whose names stay in the map.
template <class T> T* NewList(unsigned int count) {
    void* heap = AtModifierDatabase::s_heap;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    T* list = AtNewArray<T>(count);
    AtPopHeap();
    if (list == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return list;
}

template <class T> void Free(T* list) {
    if (list != 0) {
        AtDeleteArray(list);
    }
}

}  // namespace

void AtModifierDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtModifierDatabase::AtModifierDatabase() {
    m_lightCount = 0;
    m_ambientCount = 0;
    m_contextCount = 0;
    m_perspectiveCount = 0;
    m_groupCount = 0;
    m_blockCount = 0;
    m_lights = 0;
    m_ambients = 0;
    m_contexts = 0;
    m_perspectives = 0;
    m_groups = 0;
    m_blocks = 0;
}

AtModifierDatabase::~AtModifierDatabase() {
    AtModifierDatabase::Clear();
}

void AtModifierDatabase::Clear() {
    Free(m_lights);
    Free(m_contexts);
    Free(m_perspectives);
    Free(m_ambients);
    Free(m_groups);
    Free(m_blocks);
    GmNameMap::Clear();
    m_lightCount = 0;
    m_ambientCount = 0;
    m_contextCount = 0;
    m_perspectiveCount = 0;
    m_groupCount = 0;
    m_blockCount = 0;
    m_lights = 0;
    m_ambients = 0;
    m_contexts = 0;
    m_perspectives = 0;
    m_groups = 0;
    m_blocks = 0;
}

void AtModifierDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASM", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASM");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtModifierDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char ns[8];
    const char* space = 0;
    if (tokenizer->Next() == kAtWordModifierNameSpace) {
        tokenizer->ExpectName(ns);
        space = ns;
        // The word that follows is not looked at; a file has Modifier here. So is the first word of a file with no name space.
        tokenizer->Next();
    }
    tokenizer->ExpectLeftBracket();
    const int total = tokenizer->ExpectInt();
    tokenizer->ExpectRightBracket();
    if ((unsigned int)total < 1) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    Init((unsigned int)total);
    // 0x0003FFF0, AtNameTableOwner's add for this table, is in a range held elsewhere; it is these same lines. Compared through the original's own.
    if (space == 0) {
        owner->m_modifiers.Add(this);
    } else {
        owner->m_modifierSpaces.Add(space, this);
    }

    char name[8];
    for (int word = tokenizer->Next(); word != AtIoTokenizer::kEnd; word = tokenizer->Next()) {
        switch (word) {
        case kAtWordLightModifier:
            if (m_lights != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_lightCount = (unsigned int)tokenizer->ExpectCount();
            m_lights = NewList<AtLightModifier>(m_lightCount);
            for (unsigned int i = 0; i < m_lightCount; i++) {
                tokenizer->ExpectToken(kAtWordLightModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_lights[i]);
                m_lights[i].Read(owner->m_device, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordAmbientModifier:
            if (m_ambients != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_ambientCount = (unsigned int)tokenizer->ExpectCount();
            m_ambients = NewList<AtAmbientModifier>(m_ambientCount);
            for (unsigned int i = 0; i < m_ambientCount; i++) {
                tokenizer->ExpectToken(kAtWordAmbientModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_ambients[i]);
                m_ambients[i].Read(tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordPerspectiveModifier:
            if (m_perspectives != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_perspectiveCount = (unsigned int)tokenizer->ExpectCount();
            m_perspectives = NewList<AtPerspectiveModifier>(m_perspectiveCount);
            for (unsigned int i = 0; i < m_perspectiveCount; i++) {
                tokenizer->ExpectToken(kAtWordPerspectiveModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_perspectives[i]);
                m_perspectives[i].Read(tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordContextConstantModifier:
            if (m_contexts != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_contextCount = (unsigned int)tokenizer->ExpectCount();
            m_contexts = NewList<AtContextConstantModifier>(m_contextCount);
            for (unsigned int i = 0; i < m_contextCount; i++) {
                tokenizer->ExpectToken(kAtWordContextConstantModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_contexts[i]);
                m_contexts[i].Read(tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordGroupModifier:
            if (m_groups != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_groupCount = (unsigned int)tokenizer->ExpectCount();
            m_groups = NewList<AtGroupModifier>(m_groupCount);
            for (unsigned int i = 0; i < m_groupCount; i++) {
                tokenizer->ExpectToken(kAtWordGroupModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_groups[i]);
                m_groups[i].Read(owner, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordBlockModifier:
            if (m_blocks != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_blockCount = (unsigned int)tokenizer->ExpectCount();
            m_blocks = NewList<AtBlockModifier>(m_blockCount);
            for (unsigned int i = 0; i < m_blockCount; i++) {
                tokenizer->ExpectToken(kAtWordBlockModifier);
                tokenizer->ExpectName(name);
                Insert(name, &m_blocks[i]);
                m_blocks[i].Read(tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}
