// GmLevelName, GmLevelNameArray, GmLevelManager: the reader of Levels.txt. Original address clusters 0x0013D8E0-0x0013D9C2, 0x0013DFE0-0x0013E13F,
// 0x0013E1F0-0x0013E504, 0x0013E670-0x0013E6F8, 0x001950A0-0x0019510A, 0x00013F70.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmLevelManager.h"

extern "C" {
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
}

void* GmLevelNameArray::s_heap;
void* GmLevelManager::s_heap;
GmLevelManager* GmLevelManager::s_instance;

namespace {

enum LevelToken {
    kLevels = 0x39, kLevel = 0x3A, kNumLoadItems = 0x3B, kType = 0x3C, kToolCinema = 0x3D, kFmvCinema = 0x3E, kDesignerCinema = 0x3F,
    kConversationCinema = 0x40, kNormalLevel = 0x41, kHiddenLevel = 0x42, kNonSelectableLevel = 0x43, kDescription = 0x44
};

// 0x002B9198. Four of these are never acted on below: Type, ToolCinema, DesignerCinema and ConversationCinema.
const AtIoKeyword kKeywords[] = {
    {kLevels, "Levels"},
    {kLevel, "Level"},
    {kNumLoadItems, "NumLoadItems"},
    {kType, "Type"},
    {kToolCinema, "ToolCinema"},
    {kFmvCinema, "FMVCinema"},
    {kDesignerCinema, "DesignerCinema"},
    {kConversationCinema, "ConversationCinema"},
    {kNormalLevel, "NormalLevel"},
    {kHiddenLevel, "HiddenLevel"},
    {kNonSelectableLevel, "NonSelectableLevel"},
    {kDescription, "Description"},
};

// `count * 4` in the original, for ints and for 32-bit pointers alike. The element size keeps both right on a 64-bit host.
void* AllocateTable(int count, unsigned int elementSize) {
    if (GmLevelManager::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmLevelManager::s_heap);
    void* table = operator new((unsigned int)count * elementSize);
    AtPopHeap();
    if (table == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return table;
}

}  // namespace

void AtCopyName8(char* to, const char* from) {
    if (from == 0) {
        to[0] = 0;
        return;
    }
    for (int i = 0; i < 8; i++) {
        to[i] = from[i];
    }
}

GmLevelName::GmLevelName() {
}

GmLevelName::~GmLevelName() {
}

GmLevelName& GmLevelName::operator=(const GmLevelName& other) {
    AtCopyName8(m_name, other.m_name);
    return *this;
}

void GmLevelName::SetName(const char* name) {
    AtCopyName8(m_name, name);
}

void GmLevelNameArray::Resize(unsigned int count) {
    if (count == 0) {
        delete[] m_items;
        m_items = 0;
        m_count = 0;
        return;
    }
    if (count == m_count) {
        return;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    GmLevelName* items = new GmLevelName[count];
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (m_items != 0) {
        const unsigned int keep = count < m_count ? count : m_count;
        for (unsigned int i = 0; i < keep; i++) {
            items[i] = m_items[i];
        }
        delete[] m_items;
    }
    m_items = items;
    m_count = count;
}

void GmLevelManager::SetHeap(void* heap) {
    s_heap = heap;
}

void GmLevelManager::Reset() {
    m_currentIndex = -1;
    for (int i = 0; i < 9; i++) {
        m_currentName[i] = 0;
    }
    m_previousIndex = -1;
    m_levelCount = 0;
    m_unknown05C = 0;
    m_kinds = 0;
    m_loadItems = 0;
    m_unknown060 = 0;
    m_unknown020 = 1;
    AtCopyName("Start", m_startPoint);
    for (int i = 0; i < 12; i++) {
        m_unknown02C[i] = 0;
    }
    m_loaded = 0;
}

void GmLevelManager::Load(const char* path) {
    if (m_loaded != 0) {
        Unload();
    }
    AtIoTokenFile file;
    const int status = file.Open(path, 2, 0);
    if (status != 0) {
        // Original behaviour preserved: no bound on the path. If AtFatal returns, the next line dereferences a null tokenizer.
        char message[0x80];
        char number[12];
        AtFormatInt(status, number);
        strcpy(message, "Error opening file: ");
        strcat(message, path);
        strcat(message, "  \n return code :");
        strcat(message, number);
        strcat(message, "\n");
        AtFatal(message, 0, 0, 0);
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);
    for (int token = t->Next(); token != 0; token = t->Next()) {
        if (token != kLevels) {
            continue;
        }
        m_levelCount = t->ExpectCount();
        GmLevelNameArray::s_heap = s_heap;
        m_names.Resize((unsigned int)m_levelCount);
        m_kinds = (int*)AllocateTable(m_levelCount, sizeof(int));
        m_loadItems = (int*)AllocateTable(m_levelCount, sizeof(int));
        m_descriptions = (char**)AllocateTable(m_levelCount, sizeof(char*));
        for (int i = 0; i < m_levelCount; i++) {
            t->ExpectToken(kLevel);
            char name[8];
            t->ExpectName(name);
            m_names.m_items[i].SetName(name);
            m_descriptions[i] = 0;
            switch (t->Next()) {
            case kFmvCinema: m_kinds[i] = kMovie; break;
            case kNormalLevel: m_kinds[i] = kNormal; break;
            case kHiddenLevel: m_kinds[i] = kHidden; break;
            case kNonSelectableLevel: m_kinds[i] = kNonSelectable; break;
            default: break;
            }
            if (t->Next() == AtIoTokenizer::kLeftCurly) {
                // Original behaviour preserved: the end of the data inside a block never ends this loop.
                for (int inner = t->Next(); inner != AtIoTokenizer::kRightCurly; inner = t->Next()) {
                    if (inner == kNumLoadItems) {
                        m_loadItems[i] = t->ExpectInt();
                    } else if (inner == kDescription) {
                        t->ExpectStringCopy(&m_descriptions[i]);
                    }
                }
            } else {
                t->m_pushedBack = 1;
            }
        }
        t->ExpectRightCurly();
    }
    m_loaded = 1;
}

void GmLevelManager::Unload() {
    if (m_loaded != 0) {
        ReleaseLevelData();
        for (int i = 0; i < m_levelCount; i++) {
            if (m_descriptions[i] != 0) {
                operator delete(m_descriptions[i]);
                m_descriptions[i] = 0;
            }
        }
        operator delete(m_descriptions);
        if (m_kinds != 0) {
            operator delete(m_kinds);
        }
        if (m_loadItems != 0) {
            operator delete(m_loadItems);
        }
    }
    Reset();
}

GmLevelManager::GmLevelManager() {
    m_names.m_items = 0;
    m_names.m_count = 0;
    m_descriptions = 0;
    if (s_instance == 0) {
        s_instance = this;
    }
    Reset();
}

GmLevelManager::~GmLevelManager() {
    if (s_instance == this) {
        s_instance = 0;
    }
    Unload();
    m_names.Resize(0);
}
