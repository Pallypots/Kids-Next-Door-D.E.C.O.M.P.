// See include/at/AtMatAssignment.h.
#include "at/AtMatAssignment.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtMatAssignment::s_heap;
void* AtMatAssignmentDatabase::s_heap;

// 0x002B06B0.
const AtIoKeyword AtMatAssignmentDatabase::s_keywords[AtMatAssignmentDatabase::kKeywordCount] = {
    {0x3A, "NameSpace"}, {0x3B, "MatAssignment"}, {0x3C, "MatAssignmentWithNameSpace"},
};

namespace {

enum Word { kNameSpace = 0x3A, kMatAssignment = 0x3B, kMatAssignmentWithNameSpace = 0x3C };

// A count is read, and room made for it, the same way in both reads. The heap is not cleared.
void BeginList(AtMatAssignment* list, AtIoTokenizer* tokenizer) {
    if (list->m_count > 0) {
        if (list->m_items != 0) {
            operator delete(list->m_items);
        }
        list->m_count = 0;
        list->m_items = 0;
    }
    list->m_count = (unsigned int)tokenizer->ExpectCount();
    if (AtMatAssignment::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtMatAssignment::s_heap);
    list->m_items = (void**)operator new(list->m_count * sizeof(void*));
    AtPopHeap();
    if (list->m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

}  // namespace

void AtMatAssignment::SetHeap(void* heap) {
    s_heap = heap;
}

AtMatAssignment::AtMatAssignment() {
    m_count = 0;
    m_items = 0;
}

AtMatAssignment::~AtMatAssignment() {
    if (m_items != 0) {
        operator delete(m_items);
    }
    m_count = 0;
    m_items = 0;
}

void AtMatAssignment::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns) {
    BeginList(this, tokenizer);
    for (unsigned int i = 0; i < m_count; i++) {
        char name[8];
        tokenizer->ExpectName(name);
        if (name[0] == 0) {
            m_items[i] = 0;
        } else {
            m_items[i] = owner->FindMaterial(name, ns);
        }
    }
    tokenizer->ExpectRightCurly();
}

void AtMatAssignment::ReadWithNameSpaces(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    BeginList(this, tokenizer);
    for (unsigned int i = 0; i < m_count; i++) {
        char name[8];
        char space[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(space);
        if (name[0] == 0) {
            m_items[i] = 0;
        } else {
            m_items[i] = owner->FindMaterial(name, space);
        }
    }
    tokenizer->ExpectRightCurly();
}

void AtMatAssignment::Copy(const AtMatAssignment& from) {
    if (m_count > 0) {
        if (m_items != 0) {
            operator delete(m_items);
        }
        m_count = 0;
        m_items = 0;
    }
    m_count = from.m_count;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_items = (void**)operator new(m_count * sizeof(void*));
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_count; i++) {
        m_items[i] = from.m_items[i];
    }
}

void AtMatAssignmentDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtMatAssignmentDatabase::AtMatAssignmentDatabase() {
    m_count = 0;
    m_lists = 0;
}

void AtMatAssignmentDatabase::Clear() {
    if (m_lists != 0) {
        delete[] m_lists;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_lists = 0;
}

AtMatAssignmentDatabase::~AtMatAssignmentDatabase() {
    if (m_lists != 0) {
        delete[] m_lists;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_lists = 0;
}

void AtMatAssignmentDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        token = tokenizer->Next();
        ns = space;
    }
    int eachHasSpace = 0;
    if (token == kMatAssignmentWithNameSpace) {
        eachHasSpace = 1;
    } else if (token != kMatAssignment) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_lists = new AtMatAssignment[m_count];
    AtPopHeap();
    if (m_lists == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(m_count);
    owner->AddMatAssignments(this, ns);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kMatAssignment);
        char name[8];
        tokenizer->ExpectName(name);
        Insert(name, &m_lists[i]);
        if (eachHasSpace != 0) {
            m_lists[i].ReadWithNameSpaces(owner, tokenizer);
        } else {
            m_lists[i].Read(owner, tokenizer, ns);
        }
    }
    tokenizer->ExpectRightCurly();
}

void AtMatAssignmentDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    if (m_lists != 0) {
        Clear();
    }
    const int status = file.Open(name, "AGN", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGN");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    Read(owner, tokenizer);
    file.Close();
}
