// Shared by the differential oracles for code that reads files: the call log, the original's helpers redirected, and a file and file system
// in memory beneath both sides. Include after defining eq(), eqs() and the g_ globals the including file declares; one translation unit each.
#pragma once

// ---- call log: fixed storage, so logging never allocates --------------------------------------------------------------
struct Log {
    uint32_t v[1 << 14];
    int n = 0;
    void add(uint32_t a, uint32_t b = 0) { if (n + 2 <= (int)(sizeof v / 4)) { v[n++] = a; v[n++] = b; } }
} static g_log[2];
static char g_message[2][4096];
static int g_side;
static bool g_recording;
static int g_syntax_errors[2];

static void record_message(const char* m, int side) {
    if (!g_recording) return;
    strncat_s(g_message[side], m, _TRUNCATE);
    strncat_s(g_message[side], "|", _TRUNCATE);
    if (std::strstr(m, "syntax error")) g_syntax_errors[side]++;
}

// The game's heap is not cleared, so a harness that hands out zeroes hides a missing store. Fill byte 0 means zeroes.
static int g_fill;
static bool g_fail_alloc;  // Heap requests of the code under test come back null.
static std::size_t g_heap_pad;  // Room past each block, for code under test that writes beyond what it asked for.
static void* filled(std::size_t n) { if (g_fail_alloc) return nullptr; n += g_heap_pad; void* p = std::calloc(1, n); if (p && g_fill) std::memset(p, g_fill, n); return p; }

static void* __cdecl o_new(unsigned n) { if (g_recording) g_log[0].add('N', n); return filled(n); }
static void __cdecl o_delete(void* p) { if (g_recording && p) g_log[0].add('D'); std::free(p); }
static void __cdecl o_push(void* h) { if (g_recording) g_log[0].add('P', (uint32_t)(uintptr_t)h); }
static void __cdecl o_pop() { if (g_recording) g_log[0].add('p'); }
static void __cdecl o_fatal(const char* m, int a, int, int) { if (g_recording) { g_log[0].add('F', a); record_message(m, 0); } }
static void __cdecl o_raise(int code, int, int, int) { if (g_recording) g_log[0].add('R', code); }

void* operator new(std::size_t n) { if (g_recording && g_side == 1) g_log[1].add('N', (uint32_t)n); return g_recording ? filled(n) : std::calloc(1, n); }
void* operator new[](std::size_t n) { if (g_recording && g_side == 1) g_log[1].add('N', (uint32_t)n); return g_recording ? filled(n) : std::calloc(1, n); }
void operator delete(void* p) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete[](void* p) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete(void* p, std::size_t) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void AtPushHeap(void* h) { if (g_recording) g_log[1].add('P', (uint32_t)(uintptr_t)h); }
void AtPopHeap() { if (g_recording) g_log[1].add('p'); }
void AtFatal(const char* m, int a, int, int) { if (g_recording) { g_log[1].add('F', a); record_message(m, 1); } }
void AtRaise(int code, int, int, int) { if (g_recording) g_log[1].add('R', code); }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

// Runs one call on one side with logging on. Logging is off in between, because the harness's own containers allocate.
struct On {
    explicit On(int side) { g_side = side; g_recording = true; }
    ~On() { g_recording = false; g_side = 0; }
};

static void compare_logs(const char* what) {
    g_recording = false;
    eq(what, g_log[0].n, g_log[1].n);
    if (g_log[0].n != g_log[1].n && g_failures < 3) {
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < g_log[side].n && i < 40; i += 2) std::printf("%c%X ", (char)g_log[side].v[i], g_log[side].v[i + 1]);
            std::printf("\n");
        }
    }
    for (int i = 0; i < g_log[0].n && i < g_log[1].n; i++) {
        if (g_log[0].v[i] != g_log[1].v[i]) {
            eq(what, g_log[0].v[i], g_log[1].v[i]);
            break;
        }
    }
    eqs(what, g_message[0], g_message[1]);
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
}

// ---- test doubles beneath both sides ----------------------------------------------------------------------------------
class Request : public AtIoFileRequest {
public:
    int side = 0;
    int status = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    int Wait() override { g_log[side].add('W', status); return status; }
};

enum Mode { kNormal, kPendingIo, kOneByte, kZeroAtEnd, kFailAfterFirst, kWaitFails };

class MemFile : public AtIoFile {
public:
    int side = 0;
    const unsigned char* data = nullptr;
    size_t size = 0;
    Mode mode = kNormal;
    std::string* written = nullptr;
    int reads = 0;
    Request request;
    int Close() override { g_log[side].add('c'); return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { g_log[side].add('S', offset); m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest** req) override {
        g_log[side].add('r', n);
        reads++;
        if (mode == kFailAfterFirst && reads > 1) return AtIoFile::kIoError;
        size_t have = size - m_position;
        if (n > have) n = (unsigned)have;
        if (mode == kOneByte && n > 1) n = 1;
        if (n) std::memcpy(out, data + m_position, n);
        m_position += n;
        if (done) *done = n;
        const int status = (n || mode == kZeroAtEnd) ? 0 : (int)AtIoFile::kEndOfFile;
        if (mode == kPendingIo || mode == kWaitFails) {
            request.side = side;
            request.status = mode == kWaitFails ? (int)AtIoFile::kIoError : status;
            *req = &request;
            return AtIoFile::kPending;
        }
        return status;
    }
    int Write(const void* in, unsigned int n, unsigned int* done) override {
        g_log[side].add('w', n);
        // Nothing is logged about content here: bytes are compared at the end, as one string.
        if (written) {
            const bool was = g_recording;  // The string grows on the harness's heap, not the code under test's.
            g_recording = false;
            written->append((const char*)in, n);
            g_recording = was;
        }
        m_position += n;
        if (mode == kPendingIo) {
            request.side = side;
            request.status = 0;
            *done = (unsigned int)(uintptr_t)&request;
            return AtIoFile::kPending;
        }
        if (done) *done = n;
        return 0;
    }
    int Flush() override { g_log[side].add('f'); return 5; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { g_log[side].add('e'); return 0; }
};

class MemFs : public AtIoFileSystem {
public:
    MemFile files[2];
    bool has = true;
    int open_status = 0;
    int Open(const char* path, int flags, AtIoFile** out) override {
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        MemFile& f = files[g_side];
        f.side = g_side;
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = (flags & 0x21) ? 0x0C : 2;
        *out = open_status ? nullptr : &f;
        return open_status;
    }
    int Close(AtIoFile*) override { g_log[g_side].add('C'); return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char*) override { g_log[g_side].add('M'); return has ? 0 : 1; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

