// GmManifestLoader: the base of a time-sliced file loader. Original address cluster 0x00141E10-0x001420FA.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmManifestLoader.h"
#include "gm/GmLevelManager.h"

extern "C" {
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
}

AtMemoryManager* AtMemoryManager::s_instance;

void GmManifestLoader::Configure(const GmManifestRequest* request) {
    strcpy(m_path, request->m_path);
    m_tag = request->m_tag;
    AtCopyName8(m_name, request->m_name);
}

void GmManifestLoader::Reset() {
    m_path[0] = 0;
    m_name[0] = 0;
    m_tail = 0;
    m_head = 0;
    m_tag = 0;
    m_done = 0;
    m_state = kOpening;
}

void GmManifestLoader::ReadTokens(AtIoTokenizer* tokenizer) {
    for (int token = tokenizer->Next(); token != 0; token = tokenizer->Next()) {
        OnToken(token, tokenizer);
    }
}

void GmManifestLoader::Open() {
    if (m_file.Open(m_path, "MNG", 2, 0) != 0) {
        m_state = kFailed;
        return;
    }
    if (m_name[0] != 0) {
        // The original builds this name and never uses it.
        char unused[16];
        AtCopyName9(unused, m_name);
        strcat(unused, ".txt");
    }
    m_state = kParsing;
}

GmManifestLoader::GmManifestLoader() {
    Reset();
}

GmManifestLoader::~GmManifestLoader() {
    Reset();
}

void GmManifestLoader::RunNext() {
    const int saved = AtMemoryManager::s_instance->m_unknown24C;
    AtMemoryManager::s_instance->m_unknown24C = m_tag;
    if (m_head != 0) {
        m_head->Run();
        m_head = m_head->m_next;
        if (m_head == 0) {
            m_tail = 0;
            Finished();
        }
    }
    AtMemoryManager::s_instance->m_unknown24C = saved;
}

void GmManifestLoader::Enqueue(GmManifestItem* item, int unused) {
    (void)unused;
    if (m_head == 0) {
        m_head = item;
        m_tail = item;
        return;
    }
    m_tail->m_next = item;
    item->m_next = 0;
    m_tail = item;
}

int GmManifestLoader::Update(unsigned int budgetMilliseconds) {
    const unsigned int start = AtPlatformMilliseconds();
    if (budgetMilliseconds == 0) {
        return 0;
    }
    do {
        switch (m_state) {
        case kOpening:
            Open();
            break;
        case kParsing: {
            AtIoTokenizer* tokenizer = m_file.m_tokenizer;
            if (tokenizer == 0) {
                m_file.Close();
                m_state = kFailed;
                break;
            }
            Parse(tokenizer);
            ReadTokens(tokenizer);
            m_file.Close();
            AfterParse();
            m_state = kRunning;
            break;
        }
        case kRunning:
            RunNext();
            m_done = m_head == 0;
            if (m_done != 0) {
                m_state = kDone;
            }
            break;
        case kDone:
        case kFailed:
            return 1;
        default:
            break;
        }
    } while (AtPlatformMilliseconds() - start < budgetMilliseconds);
    return 0;
}
