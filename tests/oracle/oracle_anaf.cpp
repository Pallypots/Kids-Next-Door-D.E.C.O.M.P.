// Differential oracle for the controllers, .ANA, and for binding mappers to them (Windows x86, local only). See docs/verification.md.
//
//   oracle_anaf <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ANA file listed in index.txt, loaded through the file code on both sides, and every .ANM with it, bound and unbound on real links; text written here, through a tokenizer of the harness's whose Error can stop a reader that would
// otherwise never finish; and the methods called directly. The physical controllers and the device are this file's fakes, the same class on both sides: no such class is recovered, and only their fields and the slots binding calls are used.
#define NOMINMAX
#include <windows.h>

#include <float.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtInputController.h"
#include "at/AtInputMapper.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqf(const char* what, float o, float r) {
    if (o != o && r != r) { g_checks++; return; }  // NaNs that differ in sign alike: that sign is the x87's, by operand order.
    uint32_t a, b;
    std::memcpy(&a, &o, 4);
    std::memcpy(&b, &r, 4);
    eq(what, a, b);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

// AtPushHeap comes just before each heap request on both sides: that is where the Nth request is made to fail.
#define AtPushHeap DoublePushHeap
#include "oracle_doubles.h"
#undef AtPushHeap

static int g_fail_at;
static void count_request() {
    if (g_fail_at > 0) g_fail_alloc = --g_fail_at == 0;
    else g_fail_alloc = false;
}
void AtPushHeap(void* h) { count_request(); DoublePushHeap(h); }
static void __cdecl o_push_counting(void* h) { count_request(); o_push(h); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnP1)(void*, void*, void*);
typedef void(__fastcall* FnP2)(void*, void*, void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnDbRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnInt)(void*, void*, int);
typedef int(__fastcall* FnRetInt)(void*, void*);
typedef int(__fastcall* FnP1RetInt)(void*, void*, const void*);
typedef float(__fastcall* FnFloat)(void*, void*, float);
typedef void(__fastcall* FnFloatVoid)(void*, void*, float);
typedef void(__fastcall* FnSetValue)(void*, void*, unsigned, float);
typedef void(__fastcall* FnAlloc2)(void*, void*, unsigned, unsigned);
typedef void(__fastcall* FnBind)(void*, void*, void*, void*);
typedef void(__fastcall* FnUnbind)(void*, void*, void*, int);
typedef void(__fastcall* FnTell)(void*, void*, int, void*);
typedef int(__fastcall* FnBindB)(void*, void*, void*, int, int, int);
typedef int(__fastcall* FnBindA)(void*, void*, void*, int, int);
typedef void*(__fastcall* FnFindB)(void*, void*, void*, int, int, int);
typedef void*(__fastcall* FnFindA)(void*, void*, void*, int, int);
typedef void*(__fastcall* FnSlotAt)(void*, void*, unsigned);
typedef void(__fastcall* FnSizes)(void*, void*, const void*);

static void* slot(void* object, int index) { return (*(void***)object)[index]; }

// ---- fakes beneath both sides ---------------------------------------------------------------------------------------------

// A physical controller: inputs with the ids given, and chains. Nothing of it comes from the heap under test.
class Phys : public AtInputPhysicalController {
public:
    std::vector<Input> inputs;
    std::vector<Input> axisInputs;
    std::vector<Chain> chains;
    std::vector<Chain> axisChains;
    Phys(const std::vector<int>& buttonIds, const std::vector<int>& axisIds) {
        inputs.resize(buttonIds.size() + 1);
        axisInputs.resize(axisIds.size() + 1);
        chains.resize(inputs.size());
        axisChains.resize(axisInputs.size());
        for (size_t i = 0; i < buttonIds.size(); i++) inputs[i].m_id = buttonIds[i];
        for (size_t i = 0; i < axisIds.size(); i++) axisInputs[i].m_id = axisIds[i];
        for (Chain& c : chains) c.m_first = c.m_last = nullptr;
        for (Chain& c : axisChains) c.m_first = c.m_last = nullptr;
        m_buttonInputs = inputs.data();
        m_axisInputs = axisInputs.data();
        m_buttonChains = chains.data();
        m_axisChains = axisChains.data();
        m_buttonInputCount = (unsigned)buttonIds.size();
        m_axisInputCount = (unsigned)axisIds.size();
    }
    ~Phys() override { m_buttonChains = m_axisChains = nullptr; m_started = 0; }
    int Slot0() override { return 0; }
    int CopyStates() override { return 0; }
    int Update() override { return 0; }
    unsigned int ButtonCount() override { return 0; }
    unsigned int AxisCount() override { return 0; }
    unsigned int PovCount() override { return 0; }
    AtInputAxisHandler* AxisHandler(unsigned int) override { return nullptr; }
    void PhysicalSlot0() override {}
};

class Dev : public AtInputDevice {
public:
    Phys* first = nullptr;
    Phys* second = nullptr;
    std::vector<Phys*> numbered;
    std::vector<Phys*> physical;
    unsigned connected = 0;
    Dev() { m_numberedCount = 0; }
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void Slot13() override {}
    int Update(int) override { return 0; }
    void Slot15() override {}
    void Slot16() override {}
    void Slot17() override {}
    unsigned int PhysicalCount() override { return (unsigned)physical.size(); }
    unsigned int ConnectedCount() override { return connected; }
    AtInputPhysicalController* First() override { return first; }
    AtInputPhysicalController* Second() override { return second; }
    AtInputPhysicalController* Numbered(unsigned int i) override { return numbered[i]; }
    AtInputPhysicalController* Physical(unsigned int i) override { return physical[i]; }
};

// A receiver, noting what reaches it for the side at work.
class Recorder : public AtInputReceiver {
public:
    void Slot0(void* a) override { g_log[g_side].add('0', *(uint32_t*)a); }
    void Slot1(void* a) override { g_log[g_side].add('1', *(uint32_t*)a); }
    void Slot2(void* a) override { g_log[g_side].add('2', *(uint32_t*)a); }
    void Slot3(void* a, void* b) override { g_log[g_side].add('3', (uint32_t)(uintptr_t)a); g_log[g_side].add('3', (uint32_t)(uintptr_t)b); }
    void Unbind(AtInputController* c, int id) override { g_log[g_side].add('4', c != nullptr); g_log[g_side].add('4', id); }
};

// The base by itself. A vftable of the harness's is enough for what is called on it.
class Plain : public AtInputController {
public:
    int Slot0() override { return 0; }
    int CopyStates() override { return 0; }
    int Update() override { return 0; }
    unsigned int ButtonCount() override { return 0; }
    unsigned int AxisCount() override { return 0; }
    unsigned int PovCount() override { return 0; }
    AtInputAxisHandler* AxisHandler(unsigned int) override { return nullptr; }
};

// The base again, its virtual calls noted: nothing is derived from it in what is recovered, so only this can see them. Original code runs on it through this vftable.
class Noting : public Plain {
public:
    void Acquire() override { g_log[g_side].add('a'); AtInputController::Acquire(); }
    void Unacquire() override { g_log[g_side].add('u'); AtInputController::Unacquire(); }
    int SetUnknown1C(int value) override { g_log[g_side].add('S', value); return AtInputController::SetUnknown1C(value); }
};

// A handler, its setters noted. These store what the copy before them already stored, so only here do they show.
class NotingHandler : public AtInputAxisHandler {
    static uint32_t bits(float v) { uint32_t b; std::memcpy(&b, &v, 4); return b; }
public:
    void Reset() override { g_log[g_side].add('r'); AtInputAxisHandler::Reset(); }
    void SetRaw(int v) override { g_log[g_side].add('w', v); AtInputAxisHandler::SetRaw(v); }
    void SetMode(int v) override { g_log[g_side].add('m', v); AtInputAxisHandler::SetMode(v); }
    float SetMinimum(float v) override { g_log[g_side].add('l', bits(v)); return AtInputAxisHandler::SetMinimum(v); }
    float SetMaximum(float v) override { g_log[g_side].add('h', bits(v)); return AtInputAxisHandler::SetMaximum(v); }
    float SetCenter(float v) override { g_log[g_side].add('c', bits(v)); return AtInputAxisHandler::SetCenter(v); }
    float SetDeadZone(float v) override { g_log[g_side].add('d', bits(v)); return AtInputAxisHandler::SetDeadZone(v); }
    float SetSaturation(float v) override { g_log[g_side].add('s', bits(v)); return AtInputAxisHandler::SetSaturation(v); }
};

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_chain(const AtInputController::Chain& a, const AtInputController::Chain& b) {
    const AtInputLink* x = a.m_first;
    const AtInputLink* y = b.m_first;
    const AtInputLink* px = nullptr;
    const AtInputLink* py = nullptr;
    int guard = 0;
    while (x && y && guard++ < 10000) {
        eq("link's id", x->m_id, y->m_id);
        eq("link's state", x->m_state, y->m_state);
        eq("link's abstract", x->m_abstract, y->m_abstract);
        eq("link's prev", x->m_prev == px, y->m_prev == py);
        eq("link's owner", x->m_owner != nullptr, y->m_owner != nullptr);
        px = x; py = y;
        x = x->m_next; y = y->m_next;
    }
    eq("chain's length", x != nullptr, y != nullptr);
    eq("chain's last", a.m_last == px, b.m_last == py);
}

static void same_queue(const AtInputEventQueue* o, const AtInputEventQueue* r, unsigned count) {
    eq("queue's vftable", *(uint32_t*)o, 0x00277BA8);
    eq("queue's next", o->m_next != nullptr, r->m_next != nullptr);
    eq("queue's mask", o->m_mask, r->m_mask);
    eq("queue's count", o->m_count, r->m_count);
    eq("queue's events", o->m_events != nullptr, r->m_events != nullptr);
    eq("queue's nodes", o->m_nodes != nullptr, r->m_nodes != nullptr);
    if (!o->m_events || !r->m_events || !o->m_nodes || !r->m_nodes) return;
    const auto index = [](const AtInputEventQueue* q, const AtInputEventQueue::Node* n) -> long long { return n ? n - q->m_nodes : -1; };
    eq("queue's first", index(o, o->m_first), index(r, r->m_first));
    eq("queue's last", index(o, o->m_last), index(r, r->m_last));
    eq("queue's free first", index(o, o->m_freeFirst), index(r, r->m_freeFirst));
    eq("queue's free last", index(o, o->m_freeLast), index(r, r->m_freeLast));
    for (unsigned i = 0; i < count; i++) {
        eq("node's next", index(o, o->m_nodes[i].m_next), index(r, r->m_nodes[i].m_next));
        eq("node's prev", index(o, o->m_nodes[i].m_prev), index(r, r->m_nodes[i].m_prev));
        eq("node's event", o->m_nodes[i].m_event - o->m_events, r->m_nodes[i].m_event - r->m_events);
    }
    if (count) eq("events", std::memcmp(o->m_events, r->m_events, count * 0x1C), 0);
}

static void same_handler(const AtInputAxisHandler* o, const AtInputAxisHandler* r) {
    eq("handler's vftable", *(uint32_t*)o, 0x002795FC);
    eq("handler's mode", o->m_mode, r->m_mode);
    eq("handler's raw", o->m_raw, r->m_raw);
    eq("handler's id", o->m_id, r->m_id);
    eq("handler's index", o->m_index, r->m_index);
    eqf("handler's minimum", o->m_minimum, r->m_minimum);
    eqf("handler's maximum", o->m_maximum, r->m_maximum);
    eqf("handler's center", o->m_center, r->m_center);
    eqf("handler's dead zone", o->m_deadZone, r->m_deadZone);
    eqf("handler's saturation", o->m_saturation, r->m_saturation);
    eqf("handler's value", o->m_value, r->m_value);
    eqf("handler's previous", o->m_previousValue, r->m_previousValue);
    eqf("handler's filtered", o->m_filtered, r->m_filtered);
    eqf("handler's previous filtered", o->m_previousFiltered, r->m_previousFiltered);
    eq("handler's flags", o->m_flags, r->m_flags);
    eq("handler's set", o->m_isSet, r->m_isSet);
}

static void same_ints(const char* what, const int* a, const int* b, unsigned count) {
    eq(what, a != nullptr, b != nullptr);
    if (a && b && count) eq(what, std::memcmp(a, b, count * 4), 0);
}

static void same_base(const AtInputController* o, const AtInputController* r) {
    eq("buttons", o->m_buttons, r->m_buttons);
    eq("axes", o->m_axes, r->m_axes);
    eq("povs", o->m_povs, r->m_povs);
    eq("10", o->m_unknown10, r->m_unknown10);
    eq("14", o->m_unknown14, r->m_unknown14);
    eq("1C", o->m_unknown1C, r->m_unknown1C);
    eq("20", o->m_unknown20, r->m_unknown20);
    eq("names", o->m_names != nullptr, r->m_names != nullptr);
    eq("text's capacity", o->m_text.m_capacity, r->m_text.m_capacity);
    eq("text's start", o->m_text.m_start, r->m_text.m_start);
    eq("text's end", o->m_text.m_end, r->m_text.m_end);
    eq("text's data", o->m_text.m_data != nullptr, r->m_text.m_data != nullptr);
    eq("receivers' capacity", o->m_receivers.m_capacity, r->m_receivers.m_capacity);
    eq("receivers' count", o->m_receivers.m_count, r->m_receivers.m_count);
    eq("receivers' room", o->m_receivers.m_items != nullptr, r->m_receivers.m_items != nullptr);
    eq("5C", std::memcmp(o->m_unknown5C, r->m_unknown5C, sizeof o->m_unknown5C), 0);
    eq("88", std::memcmp(o->m_unknown88, r->m_unknown88, sizeof o->m_unknown88), 0);
    eq("every input", o->m_buttonInputCount, r->m_buttonInputCount);
    eq("axis inputs", o->m_axisInputCount, r->m_axisInputCount);
    eq("B4", o->m_unknownB4, r->m_unknownB4);
    eq("B8", o->m_unknownB8, r->m_unknownB8);
    eq("BC", o->m_unknownBC, r->m_unknownBC);
    eq("started", o->m_started, r->m_started);
    eq("acquired", o->m_acquired, r->m_acquired);
    eq("C8", o->m_unknownC8, r->m_unknownC8);
    eq("CC", o->m_unknownCC, r->m_unknownCC);
    eq("D0", o->m_unknownD0, r->m_unknownD0);
    const unsigned total = o->m_buttons + o->m_axes + o->m_povs;
    eq("input block", o->m_inputs != nullptr, r->m_inputs != nullptr);
    eq("button inputs", o->m_buttonInputs - o->m_inputs, r->m_buttonInputs - r->m_inputs);
    eq("axis inputs at", o->m_axisInputs - o->m_inputs, r->m_axisInputs - r->m_inputs);
    eq("pov inputs at", o->m_povInputs - o->m_inputs, r->m_povInputs - r->m_inputs);
    if (o->m_inputs && r->m_inputs && total) eq("inputs", std::memcmp(o->m_inputs, r->m_inputs, total * sizeof(AtInputController::Input)), 0);
    same_ints("states", o->m_states, r->m_states, total);
    same_ints("previous states", o->m_previousStates, r->m_previousStates, total);
    same_ints("84", o->m_unknown84, r->m_unknown84, total);
    same_ints("pov directions", o->m_povDirections, r->m_povDirections, o->m_povs);
    same_ints("pov values", (const int*)o->m_povValues, (const int*)r->m_povValues, o->m_povs);
    same_ints("previous pov values", (const int*)o->m_previousPovValues, (const int*)r->m_previousPovValues, o->m_povs);
    eq("chains", o->m_buttonChains != nullptr, r->m_buttonChains != nullptr);
    eq("axis chains", o->m_axisChains != nullptr, r->m_axisChains != nullptr);
    if (o->m_buttonChains && r->m_buttonChains) {
        for (unsigned i = 0; i < o->m_buttonInputCount; i++) same_chain(o->m_buttonChains[i], r->m_buttonChains[i]);
    }
    if (o->m_axisChains && r->m_axisChains) {
        for (unsigned i = 0; i < o->m_axisInputCount; i++) same_chain(o->m_axisChains[i], r->m_axisChains[i]);
    }
}

static void same_controller(const AtAbstractController* o, const AtAbstractController* r) {
    eq("controller's vftable", *(uint32_t*)o, 0x002786C0);
    same_base(o, r);
    eq("handlers", o->m_handlers != nullptr, r->m_handlers != nullptr);
    if (o->m_handlers && r->m_handlers) {
        eq("handlers' cookie", ((uint32_t*)o->m_handlers)[-1], ((uint32_t*)r->m_handlers)[-1]);
        for (unsigned i = 0; i < o->m_axes; i++) same_handler(&o->m_handlers[i], &r->m_handlers[i]);
    }
    same_queue(&o->m_queue, &r->m_queue, o->m_buttonInputCount * 2);
    eq("100", std::memcmp(o->m_unknown100, r->m_unknown100, sizeof o->m_unknown100), 0);
    eq("110", std::memcmp(o->m_unknown110, r->m_unknown110, sizeof o->m_unknown110), 0);
    eq("pov angles", std::memcmp((const void*)(uintptr_t)0x00301B08, AtAbstractController::s_povAngles, sizeof AtAbstractController::s_povAngles), 0);
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static unsigned g_controllers, g_inputs;

static void same_db(const char* what, const AtInputControllerDatabase* o, const AtInputControllerDatabase* r, AtInputOwner* oo, AtInputOwner* ro) {
    g_recording = false;
    eq("18", (uintptr_t)o->m_unknown18, (uintptr_t)r->m_unknown18);
    eq("1C", o->m_unknown1C, r->m_unknown1C);
    eq("items", o->m_items != nullptr, r->m_items != nullptr);
    eq("size", o->m_size, r->m_size);
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &oo->m_controllers, r->m_group == &ro->m_controllers);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_items && r->m_items) {
        const unsigned count = ((unsigned*)o->m_items)[-1];
        eq("cookie", count, ((unsigned*)r->m_items)[-1]);
        if (count == ((unsigned*)r->m_items)[-1]) {
            for (unsigned i = 0; i < count; i++) same_controller(&o->m_items[i], &r->m_items[i]);
            if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
                for (unsigned i = 0; i < o->m_size; i++) {
                    const GmNameMap::Entry& x = o->m_entries[i];
                    const GmNameMap::Entry& y = r->m_entries[i];
                    eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
                    if (x.m_value && y.m_value) {
                        eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                        eq("slot's controller", (AtAbstractController*)x.m_value - o->m_items, (AtAbstractController*)y.m_value - r->m_items);
                    }
                }
            }
        }
    }
    compare_logs(what);
}

// ---- the sides --------------------------------------------------------------------------------------------------------

struct Owners {
    AtInputOwner* side[2];
    Owners(void* a = nullptr, void* b = nullptr) {
        void* devices[2] = {a, b};
        for (int i = 0; i < 2; i++) {
            side[i] = new AtInputOwner;
            side[i]->m_unknown00 = 0;
            side[i]->m_device = devices[i];
            side[i]->m_mapperSpaces.Init(8);
            side[i]->m_controllerSpaces.Init(8);
        }
    }
    ~Owners() {
        g_recording = false;
        delete side[0];
        delete side[1];
    }
};

static AtInputControllerDatabase* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtInputControllerDatabase));
    std::memset(o, fill, sizeof(AtInputControllerDatabase));
    eq("constructor returns this", (uintptr_t)at<FnCtor>(0x000735D0)(o, nullptr), (uintptr_t)o);
    eq("database vftable", *(uint32_t*)o, 0x00277C18);
    return (AtInputControllerDatabase*)o;
}
static AtInputControllerDatabase* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(AtInputControllerDatabase));
    std::memset(r, fill, sizeof(AtInputControllerDatabase));
    return new (r) AtInputControllerDatabase;
}
static void destroy(AtInputControllerDatabase* o, AtInputControllerDatabase* r, const char* what, int flags) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00073760)(o, nullptr, flags), (uintptr_t)o); }
    { On on(1); if (flags & 1) { delete r; } else { r->~AtInputControllerDatabase(); } }
    compare_logs(what);
    if (!(flags & 1)) { std::free(o); std::free(r); }
}

static AtInputMapperDatabase* make_mo() {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtInputMapperDatabase));
    std::memset(o, 0xCD, sizeof(AtInputMapperDatabase));
    at<FnCtor>(0x000737F0)(o, nullptr);
    return (AtInputMapperDatabase*)o;
}
static AtInputMapperDatabase* make_mr() {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(AtInputMapperDatabase));
    std::memset(r, 0xCD, sizeof(AtInputMapperDatabase));
    return new (r) AtInputMapperDatabase;
}

// ---- written text, through a tokenizer that can stop --------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    int m_errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++m_errors > 12) *(volatile int*)8 = 0;  // Does not come back, as in the game. guarded() takes it from here.
    }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const std::string& t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void note_counts(const AtInputControllerDatabase* r) {
    if (!r->m_items) return;
    const unsigned count = ((unsigned*)r->m_items)[-1];
    g_controllers += count;
    for (unsigned i = 0; i < count; i++) g_inputs += r->m_items[i].m_buttonInputCount;
}

static void run_text(const std::string& text, unsigned char fill, int fail_at = 0) {
    g_context = "text [" + text + "]" + (fail_at ? " request " + std::to_string(fail_at) : "");
    Owners own;
    g_fill = fill;
    AtInputControllerDatabase* o = make_o(fill);
    AtInputControllerDatabase* r = make_r(fill);
    bool crashed = false;
    for (int pass = 0; pass < 2 && !crashed; pass++) {
        Direct da(text), db(text);
        g_fail_at = fail_at;
        const bool oc = guarded([&] { On on(0); at<FnDbRead>(0x000733C0)(o, nullptr, own.side[0], &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        g_fail_at = fail_at;
        const bool rc = guarded([&] { On on(1); r->Read(own.side[1], &db.tokenizer); });
        g_recording = false;
        g_side = 0;
        g_fail_at = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        crashed = oc || rc;
        compare_logs("read");
        if (!crashed) same_db("read", o, r, own.side[0], own.side[1]);
        eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
    }
    if (crashed) return;  // What was half made stays: a crash here has no way back in the game.
    note_counts(r);
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", 0);
}

static std::string one(const std::string& body) { return "AbstractController[1]\n{\nAbstractController \"A\"\n{\n" + body + "\n}\n}\n"; }

static void run_written() {
    const std::vector<std::string> texts = {
        "AbstractController[0]\n{\n}\n",
        one(""),
        one("Buttons 4"),
        one("Axes 3"),
        one("POVs 2"),
        one("EventHandlers 2"),
        one("DeadZone 12.5"),
        one("Saturation 80.25"),
        one("Buttons 4 Axes 3 POVs 2 EventHandlers 2 DeadZone 12.5 Saturation 80.25"),
        one("POVs 2 Axes 3 Buttons 4 Saturation 80.25 DeadZone 12.5 EventHandlers 2"),
        one("Buttons 4 Buttons 2 Axes 3 Axes 1 POVs 2 POVs 1"),
        one("Axes 2 DeadZone 90.0 Saturation 50.0"),  // A dead zone above the saturation.
        one("Axes 2 DeadZone 50.0 Saturation 50.0"),
        one("Axes 2 Saturation 0.5"),
        one("Axes 1 POVs 1 Buttons 1 DeadZone 3 Saturation 4"),  // Integers where it reads reals.
        one("Axes 1 DeadZone 25.0"),
        one("POVs 3 Buttons 1"),  // POVs and no axes: no second chains.
        one("Bogus"),
        one("Buttons"),
        one("Buttons 1 AbstractController"),
        one("NameSpace"),
        one("Buttons 1.5"),
        one("DeadZone \"text\""),
        one("Saturation Buttons"),
        "NameSpace \"Space\"\nAbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\n}\n",
        "NameSpace \"\"\nAbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\n}\n",
        "NameSpace \"Space\"\nButtons[1]\n{\n}\n",
        "Buttons[1]\n{\n}\n",
        "AbstractController[1]\n{\nButtons \"A\"\n{\n}\n}\n",
        "AbstractController[2]\n{\nAbstractController \"LongerThanEight\"\n{\nButtons 1\n}\nAbstractController \"LongerThanEightToo\"\n{\nAxes 1\n}\n}\n",
        "AbstractController[2]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\nAbstractController \"A\"\n{\nButtons 2\n}\n}\n",
        "AbstractController[3]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\nAbstractController \"B\"\n{\nAxes 2\n}\nAbstractController \"C\"\n{\nPOVs 2\n}\n}\n",
        "AbstractController[2]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\n}\n",  // Fewer than it says.
        "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\nAbstractController \"B\"\n{\n}\n}\n",  // More.
        "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\n}\nExtra",
        "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n}\n",
        "AbstractController[1]\n{\nAbstractController \"A\"\n}\n",
        "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 1\n",
        "AbstractController[1]\n",
        "",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : texts) run_text(t, fill);
    }
}

// Each heap request in turn comes back null: a raise, then what the code does with null.
static void run_no_room() {
    const std::string text = "AbstractController[2]\n{\nAbstractController \"A\"\n{\nButtons 2 Axes 2 POVs 2 EventHandlers 1\n}\nAbstractController \"B\"\n{\nButtons 1\n}\n}\n";
    for (int n = 1; n <= 40; n++) run_text(text, 0xCD, n);
    const std::string text2 = "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 2 Axes 2 EventHandlers 1\n}\n}\n";
    for (int n = 1; n <= 20; n++) run_text(text2, 0x33, n);
}

static void set_heaps(void* db, void* base, void* queue, void* list) {
    at<void(__cdecl*)(void*)>(0x000733B0)(db);
    AtInputControllerDatabase::SetHeap(db);
    eq("database heap", (uintptr_t)*(void**)(uintptr_t)0x002CF150, (uintptr_t)AtInputControllerDatabase::s_heap);
    at<void(__cdecl*)(void*)>(0x00077100)(base);
    AtInputController::SetHeap(base);
    eq("controller heap", (uintptr_t)*(void**)(uintptr_t)0x00301AF0, (uintptr_t)AtInputController::s_heap);
    at<void(__cdecl*)(void*)>(0x000726E0)(queue);
    AtInputEventQueue::SetHeap(queue);
    eq("queue heap", (uintptr_t)*(void**)(uintptr_t)0x002CF148, (uintptr_t)AtInputEventQueue::s_heap);
    at<void(__cdecl*)(void*)>(0x00078940)(list);
    AtInputLinkList::SetHeap(list);
    eq("list heap", (uintptr_t)*(void**)(uintptr_t)0x00301B00, (uintptr_t)AtInputLinkList::s_heap);
}

static void run_no_heap() {
    int other[4];
    void* one_each[4] = {&other[0], &other[1], &other[2], &other[3]};
    set_heaps(one_each[0], one_each[1], one_each[2], one_each[3]);
    const std::string text = "AbstractController[1]\n{\nAbstractController \"A\"\n{\nButtons 2 Axes 2 POVs 2 EventHandlers 1\n}\n}\n";
    run_text(text, 0xCD);  // A heap of its own to each: the push of each is logged by its token.
    for (int missing = 0; missing < 4; missing++) {
        void* h[4] = {one_each[0], one_each[1], one_each[2], one_each[3]};
        h[missing] = nullptr;
        set_heaps(h[0], h[1], h[2], h[3]);
        run_text(text, 0xCD);
    }
    set_heaps(&g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);
}

// ---- a handler -------------------------------------------------------------------------------------------------------------

static const float kValues[] = {-1e20f, -12000.0f, -10000.0f, -9500.0f, -8000.5f, -5000.0f, -1500.5f, -1500.0f, -1499.0f, -6.0f, -5.5f, -5.0f, -4.999f, -0.5f, 0.0f, 0.25f, 4.99f, 5.0f, 5.01f, 10.0f, 1499.0f, 1500.0f, 1501.0f, 5000.25f, 8000.0f, 9500.0f, 9999.0f, 10000.0f, 12000.0f, 1e20f, INFINITY, -INFINITY, NAN};

struct HandlerPair {
    unsigned char oa[0x40], ra[0x40];
    AtInputAxisHandler* o;
    AtInputAxisHandler* r;
    explicit HandlerPair(unsigned char fill) {
        std::memset(oa, fill, sizeof oa);
        std::memset(ra, fill, sizeof ra);
        eq("handler's ctor returns this", (uintptr_t)at<FnCtor>(0x0007DC00)(oa, nullptr), (uintptr_t)oa);
        o = (AtInputAxisHandler*)oa;
        r = new (ra) AtInputAxisHandler;
    }
};

static void run_handler() {
    g_context = "handler";
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        HandlerPair h(fill);
        same_handler(h.o, h.r);
        // Everything set to something, for Reset to clear.
        const auto scribble = [](AtInputAxisHandler* x, int set) {
            x->m_mode = 7; x->m_raw = 8; x->m_id = 9; x->m_index = 10;
            x->m_minimum = 1; x->m_maximum = 2; x->m_center = 3; x->m_deadZone = 4; x->m_saturation = 5;
            x->m_value = 6; x->m_previousValue = 7; x->m_filtered = 8; x->m_previousFiltered = 9;
            x->m_flags = 3; x->m_isSet = set;
        };
        for (int set = 0; set < 2; set++) {
            scribble(h.o, set);
            scribble(h.r, set);
            at<Fn0>((uint32_t)(uintptr_t)slot(h.o, 1))(h.o, nullptr);
            h.r->Reset();
            same_handler(h.o, h.r);
        }
        // The slots, by the vftable, and what they give back.
        at<FnInt>((uint32_t)(uintptr_t)slot(h.o, 2))(h.o, nullptr, 21);
        h.r->SetRaw(21);
        at<FnInt>((uint32_t)(uintptr_t)slot(h.o, 3))(h.o, nullptr, 22);
        h.r->SetMode(22);
        const float given[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f};
        eqf("set minimum", at<FnFloat>((uint32_t)(uintptr_t)slot(h.o, 4))(h.o, nullptr, given[0]), h.r->SetMinimum(given[0]));
        eqf("set maximum", at<FnFloat>((uint32_t)(uintptr_t)slot(h.o, 5))(h.o, nullptr, given[1]), h.r->SetMaximum(given[1]));
        eqf("set center", at<FnFloat>((uint32_t)(uintptr_t)slot(h.o, 6))(h.o, nullptr, given[2]), h.r->SetCenter(given[2]));
        eqf("set dead zone", at<FnFloat>((uint32_t)(uintptr_t)slot(h.o, 7))(h.o, nullptr, given[3]), h.r->SetDeadZone(given[3]));
        eqf("set saturation", at<FnFloat>((uint32_t)(uintptr_t)slot(h.o, 8))(h.o, nullptr, given[4]), h.r->SetSaturation(given[4]));
        same_handler(h.o, h.r);

        // Setup, on one that was set up and one that was not, with each kind of setting.
        const AtInputAxisHandler::Setting settings[] = {
            {0, 0, 0x40000001, 5, -10000.0f, 10000.0f, 0.0f, 15.0f, 95.0f},
            {1, 0, 0x40000002, 6, -10000.0f, 10000.0f, 0.0f, 15.0f, 95.0f},
            {0, 1, 0x40000003, 7, -10000.0f, 10000.0f, 0.0f, 15.0f, 95.0f},
            {0, 0, 0x40000004, 8, -100.0f, 300.0f, 100.0f, 25.0f, 80.0f},
            {0, 0, 0x40000005, 9, 0.0f, 1000.0f, 400.0f, 0.0f, 100.0f},
            {0, 0, 0x40000006, 10, -10000.0f, 10000.0f, 0.0f, 90.0f, 50.0f},  // The saturation becomes 95, here and in the caller's copy.
            {0, 0, 0x40000007, 11, -10000.0f, 10000.0f, 0.0f, 50.0f, 50.0f},
            {0, 0, 0x40000008, 12, -10000.0f, 10000.0f, 0.0f, 100.0f, 0.0f},
        };
        for (int unset = 0; unset < 2; unset++) {
            for (const AtInputAxisHandler::Setting& s : settings) {
                AtInputAxisHandler::Setting so = s, sr = s;
                if (unset) { h.o->m_isSet = h.r->m_isSet = 0; h.o->m_flags = h.r->m_flags = 1; }
                else { h.o->m_flags = h.r->m_flags = 1; h.o->m_isSet = h.r->m_isSet = 1; }
                h.o->m_value = h.r->m_value = 77.0f;
                at<FnP1>(0x0007DC50)(h.o, nullptr, &so);
                h.r->Setup(&sr);
                same_handler(h.o, h.r);
                eq("setting after", std::memcmp(&so, &sr, sizeof so), 0);
                for (const unsigned flags : {2u, 0u, 3u, 1u}) {
                    h.o->m_flags = h.r->m_flags = flags;
                    for (const float v : kValues) {
                        eqf("filter", at<FnFloat>(0x0007DD60)(h.o, nullptr, v), h.r->Filter(v));
                        at<FnFloatVoid>(0x0007DEC0)(h.o, nullptr, v);
                        h.r->Apply(v);
                        same_handler(h.o, h.r);
                    }
                }
            }
        }
        // Direct: with a dead zone and saturation it did not get from Setup, wide and narrow, and centers off zero.
        for (const float center : {0.0f, 100.0f, -3000.0f}) {
            for (const float dead : {0.0f, 1.0f, 15.0f, 49.0f}) {
                for (const float saturation : {100.0f, 95.0f, 60.0f, 0.0f}) {
                    h.o->m_mode = h.r->m_mode = 0;
                    h.o->m_raw = h.r->m_raw = 0;
                    h.o->m_flags = h.r->m_flags = 2;
                    h.o->m_minimum = h.r->m_minimum = -9000.0f;
                    h.o->m_maximum = h.r->m_maximum = 11000.0f;
                    h.o->m_center = h.r->m_center = center;
                    h.o->m_deadZone = h.r->m_deadZone = dead;
                    h.o->m_saturation = h.r->m_saturation = saturation;
                    for (const float v : kValues) eqf("filter, direct", at<FnFloat>(0x0007DD60)(h.o, nullptr, v), h.r->Filter(v));
                }
            }
        }
        // A value exactly on the upper edge: it is not past it. Past a maximum below the edge the quotient would be a negative zero, which is what shows the difference.
        h.o->m_flags = h.r->m_flags = 2;
        h.o->m_center = h.r->m_center = 0.0f;
        h.o->m_maximum = h.r->m_maximum = 1048576.0f;
        h.o->m_minimum = h.r->m_minimum = -1048576.0f;
        h.o->m_deadZone = h.r->m_deadZone = 1024.0f;
        h.o->m_saturation = h.r->m_saturation = 100.0f;
        eqf("filter, on the edge", at<FnFloat>(0x0007DD60)(h.o, nullptr, 10737418.0f), h.r->Filter(10737418.0f));
        scribble(h.o, 1);
        scribble(h.r, 1);
        at<Fn0>(0x0007DF20)(h.o, nullptr);
        h.r->~AtInputAxisHandler();
        same_handler(h.o, h.r);
    }
    // Setup's calls, each in its order.
    {
        g_context = "handler: setup's calls";
        for (const AtInputAxisHandler::Setting& s : {AtInputAxisHandler::Setting{3, 4, 5, 6, -7.5f, 8.5f, 0.5f, 10.5f, 20.5f}, AtInputAxisHandler::Setting{3, 4, 5, 6, -7.5f, 8.5f, 0.5f, 90.5f, 20.5f}}) {
            NotingHandler a, b;
            AtInputAxisHandler::Setting so = s, sr = s;
            { On on(0); at<FnP1>(0x0007DC50)(&a, nullptr, &so); }
            { On on(1); b.Setup(&sr); }
            compare_logs("setup's calls");
            eq("setting after", std::memcmp(&so, &sr, sizeof so), 0);
        }
    }
    // Not set up, a destructor leaves it; set up, it clears.
    {
        g_context = "handler destroyed";
        HandlerPair h(0xCD);
        h.o->m_mode = h.r->m_mode = 5;
        at<Fn0>(0x0007DF20)(h.o, nullptr);
        h.r->~AtInputAxisHandler();
        same_handler(h.o, h.r);
    }
    // The deleting form: one, none, and an array.
    {
        g_context = "handler deleted";
        void* po = std::malloc(0x40);
        void* pr = std::malloc(0x40);
        at<FnCtor>(0x0007DC00)(po, nullptr);
        new (pr) AtInputAxisHandler;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00079440)(po, nullptr, 1), (uintptr_t)po); }
        { On on(1); delete (AtInputAxisHandler*)pr; }
        compare_logs("deleted");
        unsigned char keep[0x40];
        at<FnCtor>(0x0007DC00)(keep, nullptr);
        { On on(0); eq("not freed returns this", (uintptr_t)at<FnDel>(0x00079440)(keep, nullptr, 0), (uintptr_t)keep); }
        compare_logs("not freed");
    }
}

// ---- the queue --------------------------------------------------------------------------------------------------------------

static void run_queue() {
    g_context = "queue";
    Recorder next;
    for (const unsigned mask : {0xFFu, 1u, 2u, 0u}) {
        for (const unsigned count : {0u, 1u, 5u}) {
            for (const int chained : {0, 1}) {
                g_context = "queue " + std::to_string(mask) + " " + std::to_string(count) + " " + std::to_string(chained);
                unsigned char oa[sizeof(AtInputEventQueue)], ra[sizeof(AtInputEventQueue)];
                std::memset(oa, 0xCD, sizeof oa);
                std::memset(ra, 0xCD, sizeof ra);
                eq("queue's ctor returns this", (uintptr_t)at<FnCtor>(0x00072730)(oa, nullptr), (uintptr_t)oa);
                AtInputEventQueue* o = (AtInputEventQueue*)oa;
                AtInputEventQueue* r = new (ra) AtInputEventQueue;
                same_queue(o, r, 0);
                g_fill = 0xCD;
                // Allocate clears these, whatever they were.
                o->m_count = r->m_count = 3;
                o->m_next = r->m_next = &next;
                { On on(0); at<FnAlloc2>(0x000727A0)(o, nullptr, count, mask); }
                { On on(1); r->Allocate(count, mask); }
                same_queue(o, r, count);
                compare_logs("allocated");
                if (chained) o->m_next = r->m_next = &next;
                int event[7] = {0x11, 1, 3, 4, 5, 6, 7};
                const auto all = [&](int kind) {
                    event[0]++;
                    event[1] = kind % 3 == 0;
                    event[6] = kind;
                    { On on(0); at<FnP1>((uint32_t)(uintptr_t)slot(o, kind % 3))(o, nullptr, event); }
                    { On on(1); if (kind % 3 == 0) r->Slot0(event); else if (kind % 3 == 1) r->Slot1(event); else r->Slot2(event); }
                    same_queue(o, r, count);
                    compare_logs("event");
                };
                for (int i = 0; i < 8; i++) all(i);  // Past the room, for count 5 and under.
                for (int i = 0; i < 2; i++) {
                    int a, b;
                    { On on(0); at<FnP2>((uint32_t)(uintptr_t)slot(o, 3))(o, nullptr, &a, &b); at<FnUnbind>((uint32_t)(uintptr_t)slot(o, 4))(o, nullptr, &a, 44); }
                    { On on(1); r->Slot3(&a, &b); r->Unbind((AtInputController*)&a, 44); }
                    compare_logs("passed on");
                }
                // A flush by each mask, with events of both kinds in the queue in turn.
                for (const unsigned flush : {1u, 2u, 3u, 0u}) {
                    { On on(0); at<FnInt>(0x00072A30)(o, nullptr, (int)flush); }
                    { On on(1); r->Flush(flush); }
                    same_queue(o, r, count);
                    compare_logs("flushed");
                    if (flush == 1 || flush == 2) {
                        // The ones it passed over are still there, in order; more behind them.
                        for (int i = 0; i < 6; i++) all(i + flush);
                    }
                }
                // Destroying: pointers kept, then allocated pieces gone.
                o->m_count = r->m_count = 2;
                { On on(0); at<Fn0>(0x00072760)(o, nullptr); }
                { On on(1); r->~AtInputEventQueue(); }
                eq("after: next", o->m_next != nullptr, r->m_next != nullptr);
                eq("after: mask", o->m_mask, r->m_mask);
                eq("after: count", o->m_count, r->m_count);
                eq("after: events", o->m_events != nullptr, r->m_events != nullptr);
                eq("after: nodes", o->m_nodes != nullptr, r->m_nodes != nullptr);
                eq("after: free", o->m_freeFirst != nullptr, r->m_freeFirst != nullptr);
                eq("after: first", o->m_first != nullptr, r->m_first != nullptr);
                compare_logs("destroyed");
            }
        }
    }
    // A null next, from the constructor: no pass on.
    {
        g_context = "queue: no next";
        unsigned char oa[sizeof(AtInputEventQueue)], ra[sizeof(AtInputEventQueue)];
        at<FnCtor>(0x00072730)(oa, nullptr);
        AtInputEventQueue* o = (AtInputEventQueue*)oa;
        AtInputEventQueue* r = new (ra) AtInputEventQueue;
        int a[7] = {};
        at<FnP2>((uint32_t)(uintptr_t)slot(o, 3))(o, nullptr, a, a);
        at<FnUnbind>((uint32_t)(uintptr_t)slot(o, 4))(o, nullptr, a, 1);
        r->Slot3(a, a);
        r->Unbind(nullptr, 1);
        at<FnP1>((uint32_t)(uintptr_t)slot(o, 0))(o, nullptr, a);
        r->Slot0(a);
        eq("push with no room", at<FnP1RetInt>(0x000728D0)(o, nullptr, a), r->Push(a));
        r->~AtInputEventQueue();
        at<Fn0>(0x00072760)(o, nullptr);
    }
    // Push by itself, what it returns.
    {
        g_context = "queue: push";
        unsigned char oa[sizeof(AtInputEventQueue)], ra[sizeof(AtInputEventQueue)];
        at<FnCtor>(0x00072730)(oa, nullptr);
        AtInputEventQueue* o = (AtInputEventQueue*)oa;
        AtInputEventQueue* r = new (ra) AtInputEventQueue;
        at<FnAlloc2>(0x000727A0)(o, nullptr, 2, 3);
        r->Allocate(2, 3);
        int a[7] = {1, 2, 3, 4, 5, 6, 7};
        for (int i = 0; i < 3; i++) {
            eq("pushed", at<FnP1RetInt>(0x000728D0)(o, nullptr, a), r->Push(a));
            same_queue(o, r, 2);
        }
        at<Fn0>(0x00072760)(o, nullptr);
        r->~AtInputEventQueue();
    }
    // The deleting form.
    {
        g_context = "queue deleted";
        void* po = std::malloc(sizeof(AtInputEventQueue));
        void* pr = std::malloc(sizeof(AtInputEventQueue));
        at<FnCtor>(0x00072730)(po, nullptr);
        new (pr) AtInputEventQueue;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x000729D0)(po, nullptr, 1), (uintptr_t)po); }
        { On on(1); delete (AtInputEventQueue*)pr; }
        compare_logs("deleted");
        unsigned char keep[sizeof(AtInputEventQueue)];
        at<FnCtor>(0x00072730)(keep, nullptr);
        { On on(0); eq("not freed returns this", (uintptr_t)at<FnDel>(0x000729D0)(keep, nullptr, 0), (uintptr_t)keep); }
        compare_logs("not freed");
    }
    // No room in each of its two requests.
    for (int n = 1; n <= 2; n++) {
        g_context = "queue: no room " + std::to_string(n);
        unsigned char oa[sizeof(AtInputEventQueue)], ra[sizeof(AtInputEventQueue)];
        at<FnCtor>(0x00072730)(oa, nullptr);
        AtInputEventQueue* o = (AtInputEventQueue*)oa;
        AtInputEventQueue* r = new (ra) AtInputEventQueue;
        g_fail_at = n;
        const bool oc = guarded([&] { On on(0); at<FnAlloc2>(0x000727A0)(o, nullptr, 2, 3); });
        g_recording = false; g_side = 0;
        g_fail_at = n;
        const bool rc = guarded([&] { On on(1); r->Allocate(2, 3); });
        g_recording = false; g_side = 0;
        g_fail_at = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room for a queue");
    }
}

// ---- a controller, called directly -----------------------------------------------------------------------------------------

struct ControllerPair {
    unsigned char oa[0x120], ra[0x120];
    AtAbstractController* o;
    AtAbstractController* r;
    explicit ControllerPair(unsigned char fill) {
        std::memset(oa, fill, sizeof oa);
        std::memset(ra, fill, sizeof ra);
        { On on(0); eq("controller's ctor returns this", (uintptr_t)at<FnCtor>(0x00079650)(oa, nullptr), (uintptr_t)oa); }
        { On on(1); new (ra) AtAbstractController; }
        o = (AtAbstractController*)oa;
        r = (AtAbstractController*)ra;
        eq("18 left", o->m_unknown18, r->m_unknown18);
        eq("construct", std::memcmp(oa + 4, ra + 4, 0x24), 0);
        same_controller(o, r);
        compare_logs("constructed");
    }
    void destroy() {
        { On on(0); at<Fn0>(0x00079DB0)(o, nullptr); }
        { On on(1); r->~AtAbstractController(); }
        compare_logs("controller destroyed");
    }
};

static void run_controller() {
    g_fill = 0xCD;
    static Recorder recorder;
    // Init: what a caller fills in.
    for (const unsigned povs : {0u, 2u}) {
        for (const unsigned axes : {0u, 3u}) {
            for (const unsigned buttons : {0u, 4u}) {
                g_context = "controller " + std::to_string(buttons) + " " + std::to_string(axes) + " " + std::to_string(povs);
                ControllerPair p(0x33);
                AtInputControllerSetting s = {};
                s.m_eventHandlers = 2;
                s.m_deadZone = 12.5f;
                s.m_saturation = 87.5f;
                s.m_buttons = buttons;
                s.m_axes = axes;
                s.m_povs = povs;
                // What ResetFields and the like clear.
                for (AtAbstractController* c : {p.o, p.r}) { c->m_unknown10 = 9; c->m_unknown20 = 9; c->m_unknownB4 = 9; c->m_unknownB8 = 9; c->m_unknownBC = 9; c->m_unknownC8 = 9; c->m_unknownCC = 9; c->m_unknown5C[3] = 9; c->m_unknown88[2] = 9; c->m_unknown100[1] = 9; c->m_unknown110[2] = 9; }
                { On on(0); eq("init", at<FnP1RetInt>((uint32_t)(uintptr_t)slot(p.o, 18))(p.o, nullptr, &s), 0); }
                { On on(1); eq("init", p.r->Init(s), 0); }
                same_controller(p.o, p.r);
                compare_logs("init");
                // Slots, by the vftable.
                eq("slot 0", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 0))(p.o, nullptr), p.r->Slot0());
                eq("buttons", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 10))(p.o, nullptr), p.r->ButtonCount());
                eq("axes", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 11))(p.o, nullptr), p.r->AxisCount());
                eq("povs", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 12))(p.o, nullptr), p.r->PovCount());
                for (unsigned i = 0; i < axes + 2; i++) {
                    void* a = at<FnSlotAt>((uint32_t)(uintptr_t)slot(p.o, 13))(p.o, nullptr, i);
                    AtInputAxisHandler* b = p.r->AxisHandler(i);
                    eq("handler at", a != nullptr, b != nullptr);
                    if (a && b) eq("handler's place", (AtInputAxisHandler*)a - p.o->m_handlers, b - p.r->m_handlers);
                }
                eq("slot 14", at<FnP1RetInt>((uint32_t)(uintptr_t)slot(p.o, 14))(p.o, nullptr, (void*)44), p.r->SetUnknown1C(44));
                same_controller(p.o, p.r);
                at<void(__fastcall*)(void*, void*, int, int, int)>((uint32_t)(uintptr_t)slot(p.o, 15))(p.o, nullptr, 1, 2, 3);
                p.r->Slot15(1, 2, 3);
                at<FnInt>((uint32_t)(uintptr_t)slot(p.o, 16))(p.o, nullptr, 1);
                p.r->Slot16(1);
                // Copying states: only while acquired.
                for (unsigned i = 0; i < buttons + axes + povs; i++) p.o->m_states[i] = p.r->m_states[i] = 100 + i;
                eq("copied", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 1))(p.o, nullptr), p.r->CopyStates());
                same_controller(p.o, p.r);
                at<Fn0>((uint32_t)(uintptr_t)slot(p.o, 5))(p.o, nullptr);
                p.r->Unacquire();
                same_controller(p.o, p.r);
                for (unsigned i = 0; i < buttons + axes + povs; i++) p.o->m_states[i] = p.r->m_states[i] = 200 + i;
                eq("not acquired", at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 1))(p.o, nullptr), p.r->CopyStates());
                same_controller(p.o, p.r);
                at<Fn0>((uint32_t)(uintptr_t)slot(p.o, 4))(p.o, nullptr);
                p.r->Acquire();
                same_controller(p.o, p.r);
                // SetValue on every index, an axis's raw and filtered, and past the ends.
                for (unsigned i = 0; i < axes + povs + 2; i++) {
                    for (const float v : {3000.0f, -7000.0f, 0.5f}) {
                        at<FnSetValue>(0x00077AA0)(p.o, nullptr, i, v);
                        p.r->SetValue(i, v);
                        same_controller(p.o, p.r);
                        if (i < axes) {
                            p.o->m_handlers[i].m_raw = p.r->m_handlers[i].m_raw = 0;
                            at<FnSetValue>(0x00077AA0)(p.o, nullptr, i, v);
                            p.r->SetValue(i, v);
                            same_controller(p.o, p.r);
                            p.o->m_handlers[i].m_raw = p.r->m_handlers[i].m_raw = 1;
                        }
                    }
                }
                // A POV past the axis count of the second list: m_povs bigger than the count it was given.
                if (axes + povs) {
                    p.o->m_axisInputCount--; p.r->m_axisInputCount--;
                    at<FnSetValue>(0x00077AA0)(p.o, nullptr, axes + povs - 1, 1.25f);
                    p.r->SetValue(axes + povs - 1, 1.25f);
                    same_controller(p.o, p.r);
                    p.o->m_axisInputCount++; p.r->m_axisInputCount++;
                }
                // A pov count lowered: an index still in the second list, past the POVs.
                if (povs) {
                    p.o->m_povs--; p.r->m_povs--;
                    at<FnSetValue>(0x00077AA0)(p.o, nullptr, axes + povs - 1, 2.25f);
                    p.r->SetValue(axes + povs - 1, 2.25f);
                    same_ints("pov values", (const int*)p.o->m_povValues, (const int*)p.r->m_povValues, povs);
                    p.o->m_povs++; p.r->m_povs++;
                }
                // Receivers, told of each input on release. Added by hand; what adds them in the game is not recovered.
                eq("add", at<FnP1RetInt>(0x00078AD0)(&p.o->m_receivers, nullptr, &recorder), p.r->m_receivers.Add((AtInputLink*)static_cast<AtInputReceiver*>(&recorder)));
                eq("add", at<FnP1RetInt>(0x00078AD0)(&p.o->m_receivers, nullptr, &recorder), p.r->m_receivers.Add((AtInputLink*)static_cast<AtInputReceiver*>(&recorder)));
                eq("add when full", at<FnP1RetInt>(0x00078AD0)(&p.o->m_receivers, nullptr, &recorder), p.r->m_receivers.Add((AtInputLink*)static_cast<AtInputReceiver*>(&recorder)));
                same_controller(p.o, p.r);
                for (AtAbstractController* c : {p.o, p.r}) { c->m_unknown10 = 9; c->m_unknown20 = 9; c->m_unknownB4 = 9; c->m_unknownC8 = 9; c->m_unknown100[1] = 9; c->m_unknown110[2] = 9; }
                int ro = 0, rr = 0;
                { On on(0); ro = at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 7))(p.o, nullptr); }
                { On on(1); rr = p.r->Release(); }
                eq("released", ro, rr);
                same_controller(p.o, p.r);
                compare_logs("released");
                // Not started now: nothing.
                { On on(0); ro = at<FnRetInt>((uint32_t)(uintptr_t)slot(p.o, 7))(p.o, nullptr); }
                { On on(1); rr = p.r->Release(); }
                eq("released twice", ro, rr);
                compare_logs("released twice");
                // Init again, on what Release left. A queue keeps its lists' pointers through all this, and a new Allocate writes through them into what it freed: on both sides, so that they are kept is compared, and what they hit is
                // not, by clearing them here. Where the heap puts things differs, and so would that.
                const auto clear_stale = [&] {
                    eq("free list kept", p.o->m_queue.m_freeFirst != nullptr, p.r->m_queue.m_freeFirst != nullptr);
                    eq("free list's end kept", p.o->m_queue.m_freeLast != nullptr, p.r->m_queue.m_freeLast != nullptr);
                    eq("list kept", p.o->m_queue.m_first != nullptr, p.r->m_queue.m_first != nullptr);
                    eq("list's end kept", p.o->m_queue.m_last != nullptr, p.r->m_queue.m_last != nullptr);
                    for (AtAbstractController* c : {p.o, p.r}) c->m_queue.m_freeFirst = c->m_queue.m_freeLast = c->m_queue.m_first = c->m_queue.m_last = nullptr;
                };
                clear_stale();
                { On on(0); at<FnP1RetInt>((uint32_t)(uintptr_t)slot(p.o, 18))(p.o, nullptr, &s); }
                { On on(1); p.r->Init(s); }
                same_controller(p.o, p.r);
                compare_logs("init again");
                // And again, while started: it releases first.
                s.m_buttons = 1;
                clear_stale();
                { On on(0); at<FnP1RetInt>((uint32_t)(uintptr_t)slot(p.o, 18))(p.o, nullptr, &s); }
                { On on(1); p.r->Init(s); }
                same_controller(p.o, p.r);
                compare_logs("init while started");
                p.destroy();
            }
        }
    }
    // Calling the base's ResetFields and Start directly, and TellOfLoss on a chain nobody is on.
    {
        g_context = "controller: base";
        ControllerPair p(0xCD);
        AtInputControllerSetting s = {};
        s.m_eventHandlers = 1;
        p.o->m_unknown1C = p.r->m_unknown1C = 1;
        { On on(0); eq("started", at<FnP1RetInt>(0x000771D0)(p.o, nullptr, &s), 0); }
        { On on(1); eq("started", p.r->Start(s), 0); }
        same_controller(p.o, p.r);
        compare_logs("started");
        at<FnP1RetInt>(0x00078AD0)(&p.o->m_receivers, nullptr, &recorder);
        p.r->m_receivers.Add((AtInputLink*)static_cast<AtInputReceiver*>(&recorder));
        AtInputController::Chain none = {nullptr, nullptr};
        { On on(0); at<FnTell>(0x00077B40)(p.o, nullptr, 0x30000007, &none); }
        { On on(1); p.r->TellOfLoss(0x30000007, &none); }
        compare_logs("told");
        p.o->m_receivers.m_count = p.r->m_receivers.m_count = 0;
        p.o->m_started = p.r->m_started = 0;
        at<Fn0>(0x00077110)(p.o, nullptr);
        p.r->ResetFields();
        same_controller(p.o, p.r);
        p.destroy();
    }
    // The base's Start and Release, with their virtual calls seen; and Release again, not started, on what it would otherwise clear.
    {
        g_context = "controller: the base's calls";
        Noting a, b;
        AtInputControllerSetting s = {};
        s.m_eventHandlers = 1;
        a.m_unknown1C = b.m_unknown1C = 3;
        { On on(0); at<FnP1RetInt>(0x000771D0)(&a, nullptr, &s); }
        { On on(1); b.Start(s); }
        same_base(&a, &b);
        compare_logs("started");
        { On on(0); at<FnRetInt>(0x00077B90)(&a, nullptr); }
        { On on(1); b.AtInputController::Release(); }
        same_base(&a, &b);
        compare_logs("released");
        a.m_unknown10 = b.m_unknown10 = 9;
        a.m_unknown14 = b.m_unknown14 = 9;
        { On on(0); at<FnRetInt>(0x00077B90)(&a, nullptr); }
        { On on(1); b.AtInputController::Release(); }
        same_base(&a, &b);
        compare_logs("not started");
    }
    {
        g_context = "controller: clearing handlers";
        ControllerPair p(0xCD);
        for (float& angle : AtAbstractController::s_povAngles) angle = 9.0f;
        for (int i = 0; i < 8; i++) ((float*)(uintptr_t)0x00301B08)[i] = 9.0f;
        at<Fn0>(0x000793B0)(p.o, nullptr);
        p.r->ClearHandlers();
        same_controller(p.o, p.r);
        p.destroy();
    }
    // A controller's Start through its own slot 14 is empty; the base's keeps the value, and returns it.
    {
        g_context = "controller: the base's own";
        unsigned char oa[0xD4], ra[sizeof(Plain)];
        std::memset(oa, 0xCD, sizeof oa);
        std::memset(ra, 0xCD, sizeof ra);
        { On on(0); eq("base ctor returns this", (uintptr_t)at<FnCtor>(0x000773B0)(oa, nullptr), (uintptr_t)oa); }
        Plain* r;
        { On on(1); r = new (ra) Plain; }
        compare_logs("base constructed");
        AtInputController* o = (AtInputController*)oa;
        eq("base's vftable", *(uint32_t*)o, 0x00278580);
        same_base(o, r);
        eq("slot 14", at<FnInt>((uint32_t)(uintptr_t)slot(o, 14)) != nullptr, 1);
        eq("base's slot 14", at<FnP1RetInt>((uint32_t)(uintptr_t)slot(o, 14))(o, nullptr, (void*)55), r->SetUnknown1C(55));
        same_base(o, r);
        at<void(__fastcall*)(void*, void*, int, int, int)>((uint32_t)(uintptr_t)slot(o, 15))(o, nullptr, 1, 2, 3);
        r->Slot15(1, 2, 3);
        at<FnInt>((uint32_t)(uintptr_t)slot(o, 16))(o, nullptr, 1);
        r->Slot16(1);
        at<Fn0>(0x00077EF0)(o, nullptr);
        r->~Plain();
    }
    // The base's deleting form.
    {
        g_context = "controller: the base deleted";
        void* po = std::malloc(0xD4);
        void* pr = std::malloc(sizeof(Plain));
        at<FnCtor>(0x000773B0)(po, nullptr);
        new (pr) Plain;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00078380)(po, nullptr, 1), (uintptr_t)po); }
        { On on(1); delete (Plain*)pr; }
        compare_logs("deleted");
        unsigned char keep[0xD4];
        at<FnCtor>(0x000773B0)(keep, nullptr);
        { On on(0); eq("not freed returns this", (uintptr_t)at<FnDel>(0x00078380)(keep, nullptr, 0), (uintptr_t)keep); }
        compare_logs("not freed");
    }
    // The controller's deleting form, one by itself, without its array. The array form is what the database's Clear takes.
    {
        g_context = "controller deleted";
        void* po = std::malloc(0x120);
        void* pr = std::malloc(0x120);
        at<FnCtor>(0x00079650)(po, nullptr);
        new (pr) AtAbstractController;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00073570)(po, nullptr, 1), (uintptr_t)po); }
        { On on(1); delete (AtAbstractController*)pr; }
        compare_logs("deleted");
        unsigned char keep[0x120];
        at<FnCtor>(0x00079650)(keep, nullptr);
        { On on(0); eq("not freed returns this", (uintptr_t)at<FnDel>(0x00073570)(keep, nullptr, 0), (uintptr_t)keep); }
        compare_logs("not freed");
    }
}

// ---- binding -----------------------------------------------------------------------------------------------------------------

struct MapperPair {
    unsigned char oa[0x68], ra[0x68];
    AtInputMapper* o;
    AtInputMapper* r;
    MapperPair(void* device_o, void* device_r, unsigned buttons, unsigned axes) {
        std::memset(oa, 0xCD, sizeof oa);
        std::memset(ra, 0xCD, sizeof ra);
        at<FnCtor>(0x00072B90)(oa, nullptr);
        new (ra) AtInputMapper;
        o = (AtInputMapper*)oa;
        r = (AtInputMapper*)ra;
        AtInputMapper::Sizes so = {device_o, buttons, axes};
        AtInputMapper::Sizes sr = {device_r, buttons, axes};
        at<FnSizes>(0x00073100)(o, nullptr, &so);
        r->Allocate(sr);
    }
    void button(int physical, int state, int abstractId) {
        for (AtInputMapper* m : {o, r}) {
            AtInputButtonMap& b = m->m_buttonMaps[m->m_buttonMapsUsed++];
            b.m_physical = physical; b.m_state = state; b.m_abstract = abstractId;
        }
    }
    void axis(int physical, int abstractId) {
        for (AtInputMapper* m : {o, r}) {
            AtInputAxisMap& a = m->m_axisMaps[m->m_axisMapsUsed++];
            a.m_physical = physical; a.m_abstract = abstractId;
        }
    }
    void finish() {
        { On on(0); at<Fn0>(0x000730B0)(o, nullptr); }
        { On on(1); r->~AtInputMapper(); }
        compare_logs("mapper destroyed");
    }
};

static long long link_at(const AtInputRouter* router, const AtInputLink* link) {
    if (!link) return -1;
    const unsigned buttons = router->m_boundButtons.m_capacity, axes = router->m_boundAxes.m_capacity;
    if (router->m_buttonLinks && link >= router->m_buttonLinks && link < router->m_buttonLinks + buttons) return link - router->m_buttonLinks;
    if (router->m_axisLinks && link >= router->m_axisLinks && link < router->m_axisLinks + axes) return 0x1000 + (link - router->m_axisLinks);
    return -2;
}

static void same_routers(const AtInputRouter* o, const AtInputRouter* r, const AtInputReceiver* onext = nullptr, const AtInputReceiver* rnext = nullptr) {
    eq("function kept", *(uint32_t*)&o->m_bindFunction != 0, *(uint32_t*)&r->m_bindFunction != 0);
    eq("device", o->m_device != nullptr, r->m_device != nullptr);
    eq("passed on", o->m_passOnTo != nullptr, r->m_passOnTo != nullptr);
    if (onext || rnext) eq("passed on to", o->m_passOnTo == onext, r->m_passOnTo == rnext);
    eq("free first", link_at(o, o->m_freeFirst), link_at(r, r->m_freeFirst));
    eq("free last", link_at(o, o->m_freeLast), link_at(r, r->m_freeLast));
    eq("bound buttons", o->m_boundButtons.m_count, r->m_boundButtons.m_count);
    eq("bound axes", o->m_boundAxes.m_count, r->m_boundAxes.m_count);
    const unsigned nb = std::min(o->m_boundButtons.m_capacity, r->m_boundButtons.m_capacity), na = std::min(o->m_boundAxes.m_capacity, r->m_boundAxes.m_capacity);
    for (unsigned i = 0; i < nb; i++) {
        eq("bound button", link_at(o, o->m_boundButtons.m_items[i]), link_at(r, r->m_boundButtons.m_items[i]));
        const AtInputLink& x = o->m_buttonLinks[i];
        const AtInputLink& y = r->m_buttonLinks[i];
        eq("next", link_at(o, x.m_next), link_at(r, y.m_next));
        eq("prev", link_at(o, x.m_prev), link_at(r, y.m_prev));
        eq("id", x.m_id, y.m_id);
        eq("state", x.m_state, y.m_state);
        eq("abstract", x.m_abstract, y.m_abstract);
        eq("owner", x.m_owner == o, y.m_owner == r);
        eq("controller", x.m_controller != nullptr, y.m_controller != nullptr);
    }
    for (unsigned i = 0; i < na; i++) {
        eq("bound axis", link_at(o, o->m_boundAxes.m_items[i]), link_at(r, r->m_boundAxes.m_items[i]));
        const AtInputLink& x = o->m_axisLinks[i];
        const AtInputLink& y = r->m_axisLinks[i];
        eq("next", link_at(o, x.m_next), link_at(r, y.m_next));
        eq("prev", link_at(o, x.m_prev), link_at(r, y.m_prev));
        eq("id", x.m_id, y.m_id);
        eq("abstract", x.m_abstract, y.m_abstract);
        eq("owner", x.m_owner == o, y.m_owner == r);
        eq("controller", x.m_controller != nullptr, y.m_controller != nullptr);
    }
}

static void same_phys(Phys* a, Phys* b) {
    for (size_t i = 0; i < a->chains.size(); i++) same_chain(a->chains[i], b->chains[i]);
    for (size_t i = 0; i < a->axisChains.size(); i++) same_chain(a->axisChains[i], b->axisChains[i]);
}

static const std::vector<int> kButtonIds = {0x30000000, 0x30000001, 0x30000002, 0x30000003, 0x40000000, 0x40000001};
static const std::vector<int> kAxisIds = {0x40000000, 0x40000001, 0x40000002, 0x50000000};

static void fill_maps(MapperPair& m) {
    m.button(0x30000001, 1, 0x30000010);
    m.button(0x30000001, 0, 0x30000010);
    m.button(0x30000001, 1, 0x30000010);  // Bound already, all three words the same.
    m.button(0x30000001, 1, 0x30000011);  // Another abstract id.
    m.button(0x30000002, 1, 0x30000010);  // Another physical one.
    m.button(0x3000FFFF, 1, 0x30000012);  // No such input: the link goes back.
    m.button(0x40000000, 2, 0x40000010);
    m.axis(0x40000000, 0x40000010);
    m.axis(0x40000000, 0x40000010);  // Bound already.
    m.axis(0x40000000, 0x40000011);
    m.axis(0x40000001, 0x40000010);
    m.axis(0x4000FFFF, 0x40000012);  // No such input.
    m.axis(0x50000000, 0x50000010);
}

static void bind_both(MapperPair& m, void* co, void* cr, AtAbstractController* to, AtAbstractController* tr) {
    { On on(0); at<FnBind>(0x00073010)(m.o, nullptr, co, to); }
    { On on(1); m.r->Bind((AtInputController*)cr, tr); }
    same_routers(&m.o->m_router, &m.r->m_router, &to->m_queue, &tr->m_queue);
    compare_logs("bound");
}

static void run_binding() {
    g_fill = 0xCD;
    static Recorder recorder;
    AtInputControllerSetting setting = {};
    setting.m_eventHandlers = 1;
    setting.m_buttons = 8;
    setting.m_axes = 4;
    setting.m_povs = 1;

    // To one controller, twice over, with another between; then one input, then all, unbound on real links.
    {
        g_context = "binding: to one";
        Dev dev[2];
        Phys pa(kButtonIds, kAxisIds), pb(kButtonIds, kAxisIds), qa(kButtonIds, kAxisIds), qb(kButtonIds, kAxisIds);
        MapperPair m(&dev[0], &dev[1], 16, 16);
        ControllerPair t(0xCD);
        { On on(0); at<FnP1RetInt>((uint32_t)(uintptr_t)slot(t.o, 18))(t.o, nullptr, &setting); }
        { On on(1); t.r->Init(setting); }
        compare_logs("target");
        fill_maps(m);
        AtInputController* a = static_cast<AtInputController*>(&pa);
        AtInputController* b = static_cast<AtInputController*>(&pb);
        bind_both(m, a, b, t.o, t.r);
        same_phys(&pa, &pb);
        bind_both(m, a, b, t.o, t.r);
        same_phys(&pa, &pb);
        bind_both(m, static_cast<AtInputController*>(&qa), static_cast<AtInputController*>(&qb), t.o, t.r);
        same_phys(&pa, &pb);
        same_phys(&qa, &qb);

        // What the router passes on now reaches the queue.
        int event[7] = {1, 2, 3, 4, 5, 6, 7};
        { On on(0); at<FnP1>(0x00078B60)(&m.o->m_router, nullptr, event); at<FnP1>(0x00078B70)(&m.o->m_router, nullptr, event); at<FnP1>(0x00078B90)(&m.o->m_router, nullptr, event); }
        { On on(1); m.r->m_router.Slot0(event); m.r->m_router.Slot1(event); m.r->m_router.Slot2(event); }
        same_controller(t.o, t.r);
        compare_logs("passed on");

        // One input of it, by id: every link it has for that pair goes, buttons and second list.
        for (const int id : {0x30000001, 0x40000000, 0x12345}) {
            at<FnUnbind>(0x000791B0)(&m.o->m_router, nullptr, static_cast<AtInputController*>(&qa), id);
            m.r->m_router.Unbind(static_cast<AtInputController*>(&qb), id);
            same_routers(&m.o->m_router, &m.r->m_router);
            same_phys(&qa, &qb);
        }
        at<Fn0>(0x00078DC0)(&m.o->m_router, nullptr);
        m.r->m_router.UnbindAll();
        same_routers(&m.o->m_router, &m.r->m_router);
        same_phys(&qa, &qb);
        m.finish();
        t.destroy();
    }

    // To every controller of the device: the numbered, and the connected by their flags.
    for (const unsigned connected : {0u, 1u, 2u, 3u}) {
        g_context = "binding: to all, connected " + std::to_string(connected);
        Dev dev[2];
        std::vector<Phys*> own[2];
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < 7; i++) own[s].push_back(new Phys(kButtonIds, kAxisIds));
            dev[s].first = own[s][0];
            dev[s].second = own[s][1];
            dev[s].numbered = {own[s][2]};
            dev[s].m_numberedCount = 1;
            dev[s].physical = {own[s][3], own[s][4], own[s][5], own[s][6]};
            dev[s].connected = connected;
            own[s][3]->m_acquired = 1; own[s][3]->m_started = 0;
            own[s][4]->m_acquired = 0; own[s][4]->m_started = 1;
            own[s][5]->m_acquired = 1; own[s][5]->m_started = 1;
            own[s][6]->m_acquired = 1; own[s][6]->m_started = 1;
        }
        MapperPair m(&dev[0], &dev[1], 40, 40);
        ControllerPair t(0xCD);
        fill_maps(m);
        if (connected < 3) {
            bind_both(m, nullptr, nullptr, t.o, t.r);
            eq("a Connected call", (uintptr_t)at<FnSlotAt>(0x0007E100)(&dev[0], nullptr, 1) == (uintptr_t)dev[0].physical[3], dev[1].Connected(1) == dev[1].physical[3]);
            eq("none past the end", at<FnSlotAt>(0x0007E100)(&dev[0], nullptr, 2) != nullptr, dev[1].Connected(2) != nullptr);
            eq("the first", (uintptr_t)at<FnSlotAt>(0x0007E100)(&dev[0], nullptr, 0) == (uintptr_t)dev[0].physical[2], dev[1].Connected(0) == dev[1].physical[2]);
            for (int i = 0; i < 7; i++) same_phys(own[0][i], own[1][i]);
            m.finish();
        } else {
            // More connected said than there are: Connected gives null, and that goes on into a crash.
            const bool oc = guarded([&] { On on(0); at<FnBind>(0x00073010)(m.o, nullptr, nullptr, t.o); });
            g_recording = false; g_side = 0;
            const bool rc = guarded([&] { On on(1); m.r->Bind(nullptr, t.r); });
            g_recording = false; g_side = 0;
            eq("crash", oc, rc);
            eq("crashed", oc, 1);
            compare_logs("too many connected");
        }
        if (connected < 3) t.destroy();
    }

    // With no second, and with no free link: crashes, on both.
    for (const int kind : {0, 1}) {
        g_context = kind ? "binding: no free link" : "binding: a null controller";
        Dev dev[2];
        Phys pa(kButtonIds, kAxisIds), pb(kButtonIds, kAxisIds);
        dev[0].first = &pa;
        dev[1].first = &pb;
        MapperPair m(&dev[0], &dev[1], kind ? 1 : 8, kind ? 0 : 8);  // No axis links: the free list is one, and a button takes those too.
        ControllerPair t(0xCD);
        if (kind) {
            m.button(0x30000001, 1, 1);
            m.button(0x30000002, 1, 2);
            for (AtInputMapper* x : {m.o, m.r}) x->m_buttonMapsUsed = 2;
            // And a third, past the links: two to a map, so room for two.
            for (AtInputMapper* x : {m.o, m.r}) { x->m_buttonMaps = (AtInputButtonMap*)std::realloc(nullptr, 3 * sizeof(AtInputButtonMap)); x->m_buttonMaps[0] = {0x30000001, 1, 1}; x->m_buttonMaps[1] = {0x30000002, 1, 2}; x->m_buttonMaps[2] = {0x30000003, 1, 3}; x->m_buttonMapsUsed = 3; }
        } else {
            m.button(0x30000001, 1, 1);
        }
        const bool oc = guarded([&] { On on(0); at<FnBind>(0x00073010)(m.o, nullptr, kind ? static_cast<AtInputController*>(&pa) : nullptr, t.o); });
        g_recording = false; g_side = 0;
        const bool rc = guarded([&] { On on(1); m.r->Bind(kind ? static_cast<AtInputController*>(&pb) : nullptr, t.r); });
        g_recording = false; g_side = 0;
        eq("crash", oc, rc);
        eq("crashed", oc, 1);
        compare_logs("crash");
    }

    // Binding's pieces by themselves, for what they return.
    {
        g_context = "binding: pieces";
        Dev dev[2];
        Phys pa(kButtonIds, kAxisIds), pb(kButtonIds, kAxisIds);
        dev[0].first = &pa; dev[0].second = &pa;
        dev[1].first = &pb; dev[1].second = &pb;
        MapperPair m(&dev[0], &dev[1], 8, 8);
        AtInputRouter* o = &m.o->m_router;
        AtInputRouter* r = &m.r->m_router;
        AtInputController* a = static_cast<AtInputController*>(&pa);
        AtInputController* b = static_cast<AtInputController*>(&pb);
        eq("button", at<FnBindB>(0x00079120)(o, nullptr, a, 0x30000000, 1, 5), r->BindButton(b, 0x30000000, 1, 5));
        eq("none to bind to", *(uint32_t*)&o->m_bindFunction, 0);
        same_routers(o, r);
        eq("axis", at<FnBindA>(0x00079170)(o, nullptr, a, 0x40000000, 5), r->BindAxis(b, 0x40000000, 5));
        same_routers(o, r);
        eq("all, button", at<FnBindB>(0x00079120)(o, nullptr, nullptr, 0x30000001, 1, 6), r->BindButton(nullptr, 0x30000001, 1, 6));
        eq("function: button", *(uint32_t*)&o->m_bindFunction, 0x00078EF0);
        same_routers(o, r);
        same_phys(&pa, &pb);
        eq("all, axis", at<FnBindA>(0x00079170)(o, nullptr, nullptr, 0x40000001, 6), r->BindAxis(nullptr, 0x40000001, 6));
        eq("function: axis", *(uint32_t*)&o->m_bindFunction, 0x00078FA0);
        same_routers(o, r);
        same_phys(&pa, &pb);
        const auto li = [](AtInputRouter* x, void* l) { return link_at(x, (AtInputLink*)l); };
        for (const int physical : {0x30000000, 0x30000001, 0x30000002}) {
            for (const int state : {0, 1}) {
                for (const int abstractId : {5, 6}) {
                    for (const int same : {1, 0}) {
                        eq("found", li(o, at<FnFindB>(0x00078E50)(o, nullptr, same ? (void*)a : (void*)o, physical, state, abstractId)), link_at(r, r->FindButtonLink(same ? b : (AtInputController*)r, physical, state, abstractId)));
                    }
                }
            }
        }
        for (const int physical : {0x40000000, 0x40000001, 0x40000002}) {
            for (const int abstractId : {5, 6}) {
                for (const int same : {1, 0}) {
                    eq("found axis", li(o, at<FnFindA>(0x00078EA0)(o, nullptr, same ? (void*)a : (void*)o, physical, abstractId)), link_at(r, r->FindAxisLink(same ? b : (AtInputController*)r, physical, abstractId)));
                }
            }
        }
        // The controller's own: what they return, on an id it has and one it does not.
        for (const int id : {0x30000003, 0x77777}) {
            AtInputLink l1 = {}, l2 = {};
            l1.m_id = l2.m_id = id;
            l1.m_prev = l2.m_prev = &l1;  // For binding to clear, on an id it has.
            eq("bind button", at<FnP1RetInt>(0x000777F0)(a, nullptr, &l1), b->BindButton(&l2));
            same_phys(&pa, &pb);
            AtInputLink l3 = {}, l4 = {};
            l3.m_id = l4.m_id = id == 0x30000003 ? 0x40000002 : id;
            l3.m_prev = l4.m_prev = &l3;
            eq("bind axis", at<FnP1RetInt>(0x00077920)(a, nullptr, &l3), b->BindAxis(&l4));
            same_phys(&pa, &pb);
            b->UnbindButton(&l2); a->UnbindButton(&l1);
            b->UnbindAxis(&l4); a->UnbindAxis(&l3);
        }
        m.finish();
    }

    // The very last free link, with no axis links to fall back on: the list's end goes too.
    {
        g_context = "binding: the last link";
        Dev dev[2];
        Phys pa(kButtonIds, kAxisIds), pb(kButtonIds, kAxisIds);
        MapperPair m(&dev[0], &dev[1], 1, 0);
        AtInputRouter* o = &m.o->m_router;
        AtInputRouter* r = &m.r->m_router;
        for (const int id : {0x30000000, 0x30000001}) {
            at<FnBindB>(0x00079120)(o, nullptr, static_cast<AtInputController*>(&pa), id, 1, 5);
            r->BindButton(static_cast<AtInputController*>(&pb), id, 1, 5);
            same_routers(o, r);
        }
        m.finish();
    }
    // A controller going away, with links on it: each router unbinds, each receiver hears.
    {
        g_context = "binding: released with links";
        Dev dev[2];
        MapperPair m(&dev[0], &dev[1], 16, 16);
        ControllerPair t(0xCD), bound(0x33);
        for (ControllerPair* c : {&t, &bound}) {
            { On on(0); at<FnP1RetInt>((uint32_t)(uintptr_t)slot(c->o, 18))(c->o, nullptr, &setting); }
            { On on(1); c->r->Init(setting); }
            compare_logs("a controller");
        }
        // In this controller the second list is real, so an abstract controller can stand as what is bound to.
        fill_maps(m);
        bind_both(m, bound.o, bound.r, t.o, t.r);
        same_controller(bound.o, bound.r);
        at<FnP1RetInt>(0x00078AD0)(&bound.o->m_receivers, nullptr, &recorder);
        bound.r->m_receivers.Add((AtInputLink*)static_cast<AtInputReceiver*>(&recorder));
        { On on(0); at<FnRetInt>((uint32_t)(uintptr_t)slot(bound.o, 7))(bound.o, nullptr); }
        { On on(1); bound.r->Release(); }
        same_controller(bound.o, bound.r);
        same_routers(&m.o->m_router, &m.r->m_router);
        compare_logs("released with links");
        m.finish();
        bound.destroy();
        t.destroy();
    }
}

// ---- through the file code ------------------------------------------------------------------------------------------------

static unsigned g_files, g_mapper_files, g_binds;

static void set_file(const std::string& data) {
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
}

static void run_file(const std::string& data, const std::string& context, unsigned char fill) {
    g_context = context;
    Owners own;
    set_file(data);
    g_fill = fill;
    AtInputControllerDatabase* o = make_o(fill);
    AtInputControllerDatabase* r = make_r(fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtInputControllerDatabase) - 4), 0);
    for (int pass = 0; pass < 2; pass++) {
        { On on(0); at<FnLoad>(0x000735F0)(o, nullptr, own.side[0], "DESIGN\\THING", 77); }
        { On on(1); r->Load(own.side[1], "DESIGN\\THING", 77); }
        same_db("load", o, r, own.side[0], own.side[1]);
    }
    if (fill == 0xCD) note_counts(r);
    o->m_unknown18 = r->m_unknown18 = o;  // Nothing in the code sets them, so they are set here, to see them cleared.
    o->m_unknown1C = r->m_unknown1C = 7;
    { On on(0); at<Fn0>(0x00073690)(o, nullptr); at<Fn0>(0x00073690)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", o, r, own.side[0], own.side[1]);
    g_recording = false;
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", 0);
}

// The two files of the real game together: every mapper bound to every controller, on real links, then taken apart both ways round.
static void run_real(const std::string& ana, const std::string& anm, const std::string& context, bool controllers_first) {
    g_context = context + (controllers_first ? ": controllers go first" : ": mappers go first");
    g_fill = 0xCD;
    Dev dev[2];
    Phys pa(kButtonIds, kAxisIds), pb(kButtonIds, kAxisIds);
    dev[0].first = dev[0].second = &pa;
    dev[1].first = dev[1].second = &pb;
    Owners own(&dev[0], &dev[1]);
    AtInputControllerDatabase* co = make_o(0xCD);
    AtInputControllerDatabase* cr = make_r(0xCD);
    AtInputMapperDatabase* mo = make_mo();
    AtInputMapperDatabase* mr = make_mr();
    set_file(ana);
    { On on(0); at<FnLoad>(0x000735F0)(co, nullptr, own.side[0], "DESIGN\\CONTROL\\ABSTRACT", 0); }
    { On on(1); cr->Load(own.side[1], "DESIGN\\CONTROL\\ABSTRACT", 0); }
    same_db("controllers", co, cr, own.side[0], own.side[1]);
    set_file(anm);
    { On on(0); at<FnLoad>(0x00073AD0)(mo, nullptr, own.side[0], "DESIGN\\CONTROL\\XBOX\\XBOXMAP", 0); }
    { On on(1); mr->Load(own.side[1], "DESIGN\\CONTROL\\XBOX\\XBOXMAP", 0); }
    compare_logs("mappers");
    const unsigned controllers = ((unsigned*)cr->m_items)[-1];
    const unsigned mappers = ((unsigned*)mr->m_items)[-1];
    // Each mapper to a controller of its own turn, with the id spaces its file actually uses; and every mapper to the device too, whose controllers know no joystick ids here but the fake's.
    for (unsigned i = 0; i < mappers; i++) {
        AtAbstractController* to = &co->m_items[i % controllers];
        AtAbstractController* tr = &cr->m_items[i % controllers];
        AtInputMapper& x = mo->m_items[i];
        AtInputMapper& y = mr->m_items[i];
        for (unsigned j = 0; j < controllers; j += 5) {
            AtAbstractController* bo = &co->m_items[(i + j) % controllers];
            AtAbstractController* br = &cr->m_items[(i + j) % controllers];
            // A controller with POVs and no axes has no second chains: the original crashes on one, so it is a difference only if only one side does.
            const bool oc = guarded([&] { On on(0); at<FnBind>(0x00073010)(&x, nullptr, bo, to); });
            g_recording = false; g_side = 0;
            const bool rc = guarded([&] { On on(1); y.Bind(br, tr); });
            g_recording = false; g_side = 0;
            eq("crash", oc, rc);
            compare_logs("bound");
            if (!oc && !rc) {
                same_routers(&x.m_router, &y.m_router, &to->m_queue, &tr->m_queue);
                same_controller(bo, br);
                g_binds++;
            }
        }
        const bool oc = guarded([&] { On on(0); at<FnBind>(0x00073010)(&x, nullptr, nullptr, to); });
        g_recording = false; g_side = 0;
        const bool rc = guarded([&] { On on(1); y.Bind(nullptr, tr); });
        g_recording = false; g_side = 0;
        eq("crash", oc, rc);
        compare_logs("bound to all");
        if (!oc && !rc) {
            same_routers(&x.m_router, &y.m_router, &to->m_queue, &tr->m_queue);
            same_phys(&pa, &pb);
            g_binds++;
        }
        // Back onto a controller, so a release below has links to find.
        at<FnBind>(0x00073010)(&x, nullptr, &co->m_items[i % controllers], to);
        y.Bind(&cr->m_items[i % controllers], tr);
        same_routers(&x.m_router, &y.m_router, &to->m_queue, &tr->m_queue);
        same_phys(&pa, &pb);
    }
    if (controllers_first) {
        { On on(0); at<Fn0>(0x00073690)(co, nullptr); }
        { On on(1); cr->Clear(); }
        same_db("controllers cleared, with links", co, cr, own.side[0], own.side[1]);
        for (unsigned i = 0; i < mappers; i++) same_routers(&mo->m_items[i].m_router, &mr->m_items[i].m_router);
        { On on(0); at<Fn0>(0x00073A00)(mo, nullptr); }
        { On on(1); mr->Clear(); }
        compare_logs("mappers cleared");
    } else {
        // Each router lets go of its links first.
        { On on(0); at<Fn0>(0x00073A00)(mo, nullptr); }
        { On on(1); mr->Clear(); }
        compare_logs("mappers cleared, with links");
        same_controller(&co->m_items[0], &cr->m_items[0]);
        { On on(0); at<Fn0>(0x00073690)(co, nullptr); }
        { On on(1); cr->Clear(); }
        same_db("controllers cleared", co, cr, own.side[0], own.side[1]);
    }
    g_recording = false;
    GmNameSpaceDb::Remove(co);
    GmNameSpaceDb::Remove(cr);
    GmNameSpaceDb::Remove(mo);
    GmNameSpaceDb::Remove(mr);
    destroy(co, cr, "destroy", 0);
    at<FnDel>(0x00073B70)(mo, nullptr, 0);
    mr->~AtInputMapperDatabase();
    std::free(mo);
    std::free(mr);
}

static void run_no_file() {
    g_context = "no file";
    for (const int status : {5, 2}) {
        Owners own;
        AtInputControllerDatabase* o = make_o(0xCD);
        AtInputControllerDatabase* r = make_r(0xCD);
        g_fs->open_status = status;
        const bool oc = guarded([&] { On on(0); at<FnLoad>(0x000735F0)(o, nullptr, own.side[0], "DESIGN\\THING", 0); });
        g_recording = false; g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Load(own.side[1], "DESIGN\\THING", 0); });
        g_recording = false; g_side = 0;
        eq("crash on no tokenizer", oc, rc);
        compare_logs("no file");
        g_fs->open_status = 0;
    }
}

static void run_tables() {
    g_context = "tables";
    const AtIoKeyword* image = (const AtIoKeyword*)(uintptr_t)0x002B2828;
    for (int i = 0; i < AtInputControllerDatabase::kKeywordCount; i++) {
        eq("keyword id", image[i].m_id, AtInputControllerDatabase::s_keywords[i].m_id);
        eqs("keyword", image[i].m_name, AtInputControllerDatabase::s_keywords[i].m_name);
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static std::string read_case(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) { std::printf("cannot read %s\n", path.c_str()); std::exit(2); }
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), '\0');
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    return data;
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);  // A mutant that wrecks the heap should end at once: the error reporter can hold the executable past the next build.
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    unsigned int control;
    _controlfp_s(&control, _PC_53, _MCW_PC);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_anaf <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push_counting);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    at<void(__cdecl*)(void*)>(0x00072B70)(&g_heap_token);
    AtInputMapper::SetHeap(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x00073780)(&g_heap_token);
    AtInputMapperDatabase::SetHeap(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x00078B50)(&g_heap_token);
    AtInputRouter::SetHeap(&g_heap_token);
    set_heaps(&g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables();
    run_written();
    run_no_room();
    run_no_heap();
    run_handler();
    run_queue();
    run_controller();
    run_binding();
    run_no_file();

    g_controllers = g_inputs = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    std::vector<std::string> anas, anms;
    while (std::fgets(line, sizeof line, index)) {
        const bool is_ana = std::strstr(line, ".ANA\t") != nullptr, is_anm = std::strstr(line, ".ANM\t") != nullptr;
        if (!is_ana && !is_anm) continue;
        *std::strchr(line, '\t') = 0;
        (is_ana ? anas : anms).push_back(line);
    }
    std::fclose(index);
    for (const std::string& name : anas) {
        const std::string data = read_case(dir + "\\" + name);
        run_file(data, name, 0xCD);
        run_file(data, name, 0x33);
        g_files++;
        for (const std::string& map : anms) {
            const std::string mapData = read_case(dir + "\\" + map);
            run_real(data, mapData, name + " with " + map, true);
            run_real(data, mapData, name + " with " + map, false);
            g_mapper_files++;
        }
    }
    report_entries();
    std::printf("%s: %u .ANA files, %u controllers, %u inputs, each file on two fills and read twice on each; with %u .ANM files, %u binds on real links; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_controllers, g_inputs, g_mapper_files, g_binds, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
