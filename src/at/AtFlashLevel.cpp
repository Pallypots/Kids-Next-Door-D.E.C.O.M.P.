// See include/at/AtFlashLevel.h.
#include "at/AtFlashLevel.h"

#include "at/AtHeap.h"
#include "at/AtMemory.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

namespace {

enum Word { kLevel = 0x39, kHeapSize = 0x3A, kMaxVariableValueLength = 0x3B, kMaxVariables = 0x3C, kFontSourceLevel = 0x3D, kSelf = 0x3E, kAll = 0x3F };

// At 0x00270E4C.
const AtIoKeyword s_keywords[] = {
    {kLevel, "Level"},
    {kHeapSize, "HeapSize"},
    {kMaxVariableValueLength, "MaxVariableValueLength"},
    {kMaxVariables, "MaxVariables"},
    {kFontSourceLevel, "FontSourceLevel"},
    {kSelf, "Self"},
    {kAll, "All"},
};

}  // namespace

void AtFlashLevel::Load(const char* name, int) {
    char path[9];
    m_heapSize = 0;
    m_unknown330 = 20000;
    m_maxVariableValueLength = 1000;
    m_maxVariables = 10000;
    m_fontSourceLevel = -1;
    AtCopyName9(path, name);

    AtIoTokenFile file;
    const int status = file.Open(path, "AFL", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "AFL");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kLevel:
            tokenizer->ExpectLeftCurly();
            break;
        case kHeapSize:
            m_heapSize = (unsigned int)tokenizer->ExpectInt();
            break;
        case kMaxVariableValueLength:
            m_maxVariableValueLength = tokenizer->ExpectInt();
            break;
        case kMaxVariables:
            m_maxVariables = tokenizer->ExpectInt();
            break;
        case kFontSourceLevel: {
            // Whatever else comes here is taken as a number: the integer last read stands, if it is not one.
            const int source = tokenizer->Next();
            if (source == kSelf) {
                m_fontSourceLevel = -1;
            } else if (source == kAll) {
                m_fontSourceLevel = -2;
            } else {
                m_fontSourceLevel = tokenizer->m_int;
            }
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    file.Close();

    m_heapSize = 0;
    void* parent = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;
    if (parent == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(parent);
    m_block = operator new(m_heapSize);
    AtPopHeap();
    if (m_block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    void* heap;
    if (m_heapSize > 0) {
        heap = AtHeapManager::s_instance->CreateHeapAt(m_block, m_heapSize, 2);
    } else {
        heap = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;
    }
    m_heapInUse = heap;
    m_heap = heap;
    m_heap204 = heap;
    m_first.SetHeap(heap);
    m_second.SetHeap(m_heapInUse);
    m_heap1DC = m_heapInUse;
    m_heap150 = m_heapInUse;
}
