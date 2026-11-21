// The objects the event, network and I/O layers bring up at start-up, the I/O context's Init and its read buffer, and the heap setters the layers call.
// Subsystem at/atap; see include/at/AtApLayer.h. Original clusters 0x00019180-0x0001952E, 0x00025C10-0x00025E68, 0x000991E0-0x0009929B,
// 0x000B3940-0x000B3D59, 0x000C8F90-0x000C9469, 0x00102F40. Names are inferred.
//
// The AtIoContext and AtIoBufferPool members are here, not beside src/at/AtIoFileSystem.cpp, so that this subsystem's mutants reach them.
#include "at/AtApLayer.h"
#include "at/AtEvent.h"

#include "at/AtIoJam.h"
#include "at/AtIoTokenizer.h"
#include "at/AtText.h"

void GmSetHeap00083490(void* heap);  // 0x00083490 src/gm/GmApp.cpp (gm/app)

namespace {
struct Placement {};
}
inline void* operator new(decltype(sizeof(0)), void* where, Placement) noexcept { return where; }
inline void operator delete(void*, void*, Placement) noexcept {}

void* AtEventSystem::s_heap305918;
void* AtEventSystem::s_heap3ADF94;
void* AtEventSystem::s_heap3ADF90;
void* AtEventSystem::s_heap3ADF8C;
void* g_heap301B64;
void* g_heap3B23EC;
void* g_heap301B6C;
void* AtIoContext::s_heap;
AtIoContext* AtIoContext::s_context2C6328;
AtIoContext* AtIoContext::s_context2C633C;
void* AtIoBufferPool::s_heap;

// ---- the event layer --------------------------------------------------------------------------------------------------

AtEventNames::AtEventNames() {
}

void AtEventNames::Init(unsigned int spaces, unsigned int spaces2) {
    char empty = 0;
    m_group.SetName(&empty);
    m_group2.SetName(&empty);
    if (spaces != 0) {
        m_spaces.Init(spaces);
    }
    if (spaces2 != 0) {
        m_spaces2.Init(spaces2);
    }
}

AtEventSystem::AtEventSystem() {
    m_below = 0;
}

void AtEventSystem::Init(const AtEventConfig* config, void* below) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    SetHeaps(config);
    m_names.Init(config->m_spaces, config->m_spaces2);
}

void AtEventSystem::SetHeaps(const AtEventConfig* config) {
    AtEventDatabase::SetHeap(config->m_heap10 != 0 ? config->m_heap10 : config->m_heap);
    AtSetHeap305918(config->m_heap14 != 0 ? config->m_heap14 : config->m_heap);
    AtEventList::SetHeap(config->m_heap18 != 0 ? config->m_heap18 : config->m_heap);
    AtSetHeap3ADF94(config->m_heap1C != 0 ? config->m_heap1C : config->m_heap);
    AtSetHeap3ADF90(config->m_heap20 != 0 ? config->m_heap20 : config->m_heap);
    AtSetHeap3ADF8C(config->m_heap24 != 0 ? config->m_heap24 : config->m_heap);
    if (GmNameMap::s_heap == 0) {
        GmNameMap::SetHeap(config->m_nameMapHeap != 0 ? config->m_nameMapHeap : config->m_heap);
    }
    if (GmNameSpaceDb::s_heap == 0) {
        GmNameSpaceDb::SetHeap(config->m_nameSpaceHeap != 0 ? config->m_nameSpaceHeap : config->m_heap);
    }
}

void AtSetHeap305918(void* heap) { AtEventSystem::s_heap305918 = heap; }
void AtSetHeap3ADF94(void* heap) { AtEventSystem::s_heap3ADF94 = heap; }
void AtSetHeap3ADF90(void* heap) { AtEventSystem::s_heap3ADF90 = heap; }
void AtSetHeap3ADF8C(void* heap) { AtEventSystem::s_heap3ADF8C = heap; }
void GmNameMap::SetHeap(void* heap) { s_heap = heap; }

// ---- the network layer ------------------------------------------------------------------------------------------------

AtNetworkSystemBase::AtNetworkSystemBase() {
    m_enabled = 0;
    m_below = 0;
    m_unknown1C = 0;
}

void AtNetworkSystemBase::Init(AtNetworkConfig* config, void* below) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    m_enabled = config->m_enabled;
    for (int i = 0; i < 4; i++) {
        m_settings[i] = config->m_settings[i];
    }
}

AtNetworkSystem::AtNetworkSystem() {
}

void AtNetworkSystem::Init(AtNetworkConfig* config, void* below) {
    AtApGlobals saved;  // ORIGINAL: made and never used
    (void)saved;
    config->m_enabled = 0;  // ORIGINAL: the network is always off, whatever App asked; written into the caller's config
    AtNetworkSystemBase::Init(config, below);
}

// ---- the I/O layer ----------------------------------------------------------------------------------------------------

AtIoSystemBase::AtIoSystemBase() {
    m_below = 0;
    m_factory = 0;
    m_kind = 1;
}

void AtIoSystemBase::Init(AtIoConfig* config, void* below, void*) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    m_kind = config->m_kind;
}

void AtIoSystemBase::SetHeaps(const AtIoConfig* config) {
    AtIoJamFileSystem::SetHeap(config->m_jamHeap == 0 ? config->m_heap : config->m_jamHeap);
    AtIoJamFileSystem::SetHeaderHeap(config->m_jamHeaderHeap != 0 ? config->m_jamHeaderHeap : config->m_heap);
    AtIoContext::SetHeap(config->m_contextHeap != 0 ? config->m_contextHeap : config->m_heap);
    AtIoBufferPool::SetHeap(config->m_bufferHeap == 0 ? config->m_heap : config->m_bufferHeap);
    AtTextTable::SetHeap(config->m_textHeap != 0 ? config->m_textHeap : config->m_heap);
    void* heap = config->m_fastJamHeap != 0 ? config->m_fastJamHeap : config->m_heap;
    AtIoFastJamHeader::SetHeap(heap);
    AtSetHeap301B64(heap);
    GmSetHeap00083490(heap);
    AtIoFastAlignedJamHeader::SetHeap(heap);
    if (AtIoStream::s_heap == 0) {
        AtIoStream::SetHeap(config->m_streamHeap == 0 ? config->m_heap : config->m_streamHeap);
    }
    if (GmNameMap::s_heap == 0) {
        GmNameMap::SetHeap(config->m_nameMapHeap != 0 ? config->m_nameMapHeap : config->m_heap);
    }
    if (GmNameSpaceDb::s_heap == 0) {
        GmNameSpaceDb::SetHeap(config->m_nameSpaceHeap != 0 ? config->m_nameSpaceHeap : config->m_heap);
    }
}

void AtIoSystemBase::InitContext(AtIoSystemFactory* factory, const AtIoConfig* config) {
    m_context.Init(factory, config->m_stackSize, config->m_bufferBytes, config->m_spaces, config->m_requests);
}

AtIoSystem::AtIoSystem() {
}

void AtIoSystem::Init(AtIoConfig* config, void* below, void* memory) {
    AtIoGlobals saved;
    if (config->m_kind == 1) {
        config->m_kind = 2;
    }
    AtIoSystemBase::Init(config, below, memory);
    if (config->m_kind == 0) {
        return;
    }
    m_kind = 2;
    SetHeaps(config);
    saved.Save();
    if (config->m_stackSize == 0) {
        config->m_stackSize = 1;
    }
    if (config->m_spaces == 0) {
        config->m_spaces = 8;
    }
    m_factory = AtIoGetFactory(&saved, config);
    if (config->m_stackSize < 1) {  // ORIGINAL: both defaults again, the first as a comparison this time
        config->m_stackSize = 1;
    }
    if (config->m_spaces == 0) {
        config->m_spaces = 8;
    }
    InitContext(m_factory, config);
    saved.Restore();
}

void AtIoSystem::SetHeaps(const AtIoConfig* config) {
    AtIoSystemBase::SetHeaps(config);
    AtIoSetDeviceHeaps(config);
}

void AtIoSetDeviceHeaps(const AtIoConfig* config) {
    AtIoJamFileSystem::SetHeap(config->m_jamHeap == 0 ? config->m_heap : config->m_jamHeap);
    void* heap = config->m_jamHeaderHeap != 0 ? config->m_jamHeaderHeap : config->m_heap;
    AtSetHeap3B23EC(heap);
    AtSetHeap301B6C(heap);
    AtIoSetFactoryHeaps(config);
}

// The same as AtIoSystemBase::SetHeaps, less 0x00083490's.
void AtIoSetFactoryHeaps(const AtIoConfig* config) {
    AtIoJamFileSystem::SetHeap(config->m_jamHeap == 0 ? config->m_heap : config->m_jamHeap);
    AtIoJamFileSystem::SetHeaderHeap(config->m_jamHeaderHeap != 0 ? config->m_jamHeaderHeap : config->m_heap);
    AtIoContext::SetHeap(config->m_contextHeap != 0 ? config->m_contextHeap : config->m_heap);
    AtIoBufferPool::SetHeap(config->m_bufferHeap == 0 ? config->m_heap : config->m_bufferHeap);
    AtTextTable::SetHeap(config->m_textHeap != 0 ? config->m_textHeap : config->m_heap);
    void* heap = config->m_fastJamHeap != 0 ? config->m_fastJamHeap : config->m_heap;
    AtIoFastJamHeader::SetHeap(heap);
    AtSetHeap301B64(heap);
    AtIoFastAlignedJamHeader::SetHeap(heap);
    if (AtIoStream::s_heap == 0) {
        AtIoStream::SetHeap(config->m_streamHeap != 0 ? config->m_streamHeap : config->m_heap);
    }
    if (GmNameMap::s_heap == 0) {
        GmNameMap::SetHeap(config->m_nameMapHeap != 0 ? config->m_nameMapHeap : config->m_heap);
    }
    if (GmNameSpaceDb::s_heap == 0) {
        GmNameSpaceDb::SetHeap(config->m_nameSpaceHeap == 0 ? config->m_heap : config->m_nameSpaceHeap);
    }
}

void AtSetHeap301B64(void* heap) { g_heap301B64 = heap; }
void AtSetHeap3B23EC(void* heap) { g_heap3B23EC = heap; }
void AtSetHeap301B6C(void* heap) { g_heap301B6C = heap; }
void AtTextTable::SetHeap(void* heap) { s_heap = heap; }

// ---- the context ------------------------------------------------------------------------------------------------------

void AtIoContext::SetHeap(void* heap) {
    s_heap = heap;
}

void AtIoContext::Init(AtIoFileSystemFactory* factory, unsigned int stackSize, unsigned int bufferBytes, unsigned int spaces, unsigned int requests) {
    if (m_stack != 0) {
        Clear();
    }
    m_unknown04 = factory;
    m_stackSize = stackSize;
    char empty = 0;
    m_defaultGroup.SetName(&empty);
    m_spaces.Init(spaces);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_stack = (AtIoFileSystem**)operator new(m_stackSize * sizeof(AtIoFileSystem*));
    AtPopHeap();
    if (m_stack == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_stackSize; i++) {
        m_stack[i] = 0;
    }
    if (requests > 0) {
        m_device = factory->CreateDevice((int)requests);
    }
    m_buffers.Init(factory, bufferBytes);
    AtIoFileRef::s_context = this;
    s_context2C6328 = this;
    s_instance = this;
    s_context2C633C = this;
}

void AtIoContext::Clear() {
    if (m_stack != 0) {
        operator delete(m_stack);
    }
    if (m_device != 0) {
        ((AtIoFileSystemFactory*)m_unknown04)->DestroyDevice((AtIoContextDevice*)m_device);
    }
    m_defaultGroup.Reset();
    m_spaces.Clear();
    m_buffers.Clear();
    AtIoFileRef::s_context = 0;
    s_instance = 0;
    s_context2C633C = 0;  // ORIGINAL: s_context2C6328, set by Init, is left
    m_device = 0;
    m_unknown04 = 0;
    m_stackCount = 0;
    m_stackSize = 0;
    m_stack = 0;
}

// ---- the read buffer --------------------------------------------------------------------------------------------------

void AtIoBufferPool::SetHeap(void* heap) {
    s_heap = heap;
}

void AtIoBufferPool::Init(AtIoFileSystemFactory* factory, unsigned int bytes) {
    if (m_data != 0) {
        Clear();
    }
    const unsigned int blockSize = factory->BlockSize();
    m_blockSize = blockSize;
    if (bytes < factory->MinimumBlocks() * blockSize) {
        m_size = factory->MinimumBlocks() * m_blockSize;
        m_count = factory->MinimumBlocks();
    } else {
        m_count = bytes / blockSize;
        m_size = m_count * blockSize;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_data = (unsigned char*)operator new(m_size);
    AtPopHeap();
    if (m_data == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    // An array new: the count in the word before the blocks, each block cleared.
    const unsigned int count = m_count;
    unsigned int* block = (unsigned int*)operator new(count * sizeof(AtIoBuffer) + sizeof(unsigned int));
    AtIoBuffer* blocks = 0;
    if (block != 0) {
        *block = count;
        blocks = (AtIoBuffer*)(block + 1);
        for (unsigned int i = 0; i < count; i++) {
            AtIoBuffer* b = new (blocks + i, Placement()) AtIoBuffer;
            b->m_unknown00 = 0;
            b->m_data = 0;
            b->m_flags = 0;
            b->m_unknown09[0] = b->m_unknown09[1] = b->m_unknown09[2] = 0;
            b->m_unknown0C = 0;
            b->m_size = 0;
            b->m_unknown14 = 0;
        }
    }
    m_blocks = blocks;
    AtPopHeap();
    if (m_blocks == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_count; i++) {
        m_blocks[i].Init(m_data + m_blockSize * i, m_blockSize);
    }
}

void AtIoBufferPool::Clear() {
    if (m_data != 0) {
        operator delete(m_data);
    }
    if (m_blocks != 0) {
        unsigned int* block = (unsigned int*)m_blocks - 1;
        for (unsigned int i = *block; i-- > 0;) {  // the destructors, last first: each clears its block
            AtIoBuffer& b = m_blocks[i];
            b.m_unknown00 = 0;
            b.m_data = 0;
            b.m_flags = 0;
            b.m_unknown09[0] = b.m_unknown09[1] = b.m_unknown09[2] = 0;
            b.m_unknown0C = 0;
            b.m_size = 0;
            b.m_unknown14 = 0;
        }
        operator delete(block);
    }
    m_blocks = 0;
    m_data = 0;
    m_size = 0;
    m_blockSize = 0;
    m_count = 0;
}

void AtIoBuffer::Init(unsigned char* data, unsigned int size) {
    if ((m_flags & 4) != 0) {
        m_unknown00 = 0;
        m_data = 0;
        m_flags = 0;
        m_unknown09[0] = m_unknown09[1] = m_unknown09[2] = 0;
        m_unknown0C = 0;
        m_size = 0;
        m_unknown14 = 0;
    }
    m_unknown0C = 0;
    m_data = data;
    m_flags = 4;  // ORIGINAL: tests one byte, stores a word over the flags and the three bytes after
    m_unknown09[0] = m_unknown09[1] = m_unknown09[2] = 0;
    m_size = size;
}

// ---- what is not recovered --------------------------------------------------------------------------------------------

void AtEventNames::DeletingDestructor(unsigned int flags) { AtFatal("AtEventNames: the deleting destructor (0x00099390) is not recovered", (int)flags, 0, 0); }
void AtEventSystem::DeletingDestructor(unsigned int flags) { AtFatal("AtEventSystem: the deleting destructor (0x000B3D60) is not recovered", (int)flags, 0, 0); }
void AtEventSystem::Shutdown() { AtFatal("AtEventSystem: Shutdown (0x000B3BB0) is not recovered", 0, 0, 0); }
void AtNetworkSystemBase::DeletingDestructor(unsigned int flags) { AtFatal("AtNetworkSystem: the deleting destructor (0x000C9370) is not recovered", (int)flags, 0, 0); }
void AtNetworkSystemBase::Shutdown() { AtFatal("AtNetworkSystem: Shutdown (0x000C9340) is not recovered", 0, 0, 0); }
void AtNetworkSystem::Shutdown() { AtFatal("AtNetworkSystem: Shutdown (0x000B3B30) is not recovered", 0, 0, 0); }
void AtIoSystemBase::DeletingDestructor(unsigned int flags) { AtFatal("AtIoSystem: the deleting destructor (0x000C91E0) is not recovered", (int)flags, 0, 0); }
void AtIoSystemBase::Shutdown() { AtFatal("AtIoSystem: Shutdown (0x000C9020) is not recovered", 0, 0, 0); }
void AtIoSystem::Shutdown() { AtFatal("AtIoSystem: Shutdown (0x000B3A20) is not recovered", 0, 0, 0); }
