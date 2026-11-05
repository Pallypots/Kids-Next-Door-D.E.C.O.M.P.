// Differential behaviour oracle for the recovered socket/plug/linked-list family (Windows x86, local only).
//
// Maps the needed ranges of the owner's reference XBE at their original virtual addresses and runs the ORIGINAL machine
// code side by side with the RECOVERED source, feeding both the same pseudo-random operations and comparing return
// values and the complete object graph after every operation.
//
// This is analysis tooling, allowed by the charter as a behaviour oracle. It is not the game implementation, is not
// part of any release, and contains no original bytes: they are read at run time from the owner's file.
// A pass means "no difference observed on these sequences". It is not a proof, and it is not a byte match.
//
//   oracle_gm <default.xbe> [--seed N] [--ops N] [--mutate] [--entries FILE]
// --mutate deliberately breaks the recovered side; the run must then fail (negative control).
// --entries FILE lists function addresses, one hex VA per line. A one-shot breakpoint on each proves its entry was
// reached by this run; it says nothing about which paths inside the function ran.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "gm/GmLinkedList.h"
#include "oracle_image.h"

// ---- original image -------------------------------------------------------------------------------------------------
static const uint32_t kPool = 0x002E26F0, kCursor = 0x002E26E8;
static const int kPoolSize = GmLinkedListNode::kPoolSize;

// thiscall through __fastcall: ecx = this, edx ignored, stack arguments and callee cleanup are the same.
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnR0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef int(__fastcall* FnR1)(void*, void*, void*);
typedef void*(__fastcall* FnP1i)(void*, void*, int);
typedef void*(__cdecl* FnAlloc)();

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
static void* slot(void* obj, int k) { return (*(void***)obj)[k]; }

// ---- two worlds ------------------------------------------------------------------------------------------------------
static const int kLists = 3, kItems = 6;

struct OList { void* vptr; void* ctx; uint32_t head, tail; };
struct OItem { void* vptr; uint32_t conn; };
struct OIter { void* vptr; uint32_t socket; uint32_t cur; };
struct ONode { void* vptr; uint32_t pnext, pprev, socket, plug; int active; uint32_t next, prev; };

static OList g_ol[kLists];
static OItem g_oi[kItems];
static GmLinkedList* g_rl[kLists];
static GmPlug* g_ri[kItems];
static unsigned long long g_ops, g_checks;
static bool g_mutate;
static int g_failures;

static ONode* opool() { return (ONode*)(uintptr_t)kPool; }

// Stable identity in both worlds, so addresses can be compared: pool index / list index / item index, -1 null, -2 alien.
static int o_node(uint32_t p) { return p == 0 ? -1 : (p - kPool) / 0x20; }
static int r_node(const void* p) {
    return p == nullptr ? -1 : (int)(static_cast<const GmLinkedListNode*>(p) - GmLinkedListNode::s_pool);
}
static int r_conn(const GmConnection* c) { return c ? r_node(static_cast<const GmLinkedListNode*>(c)) : -1; }
static int o_list(uint32_t p) {
    for (int i = 0; i < kLists; i++) if ((uint32_t)(uintptr_t)&g_ol[i] == p) return i;
    return p ? -2 : -1;
}
static int r_list(const GmSocket* p) {
    for (int i = 0; i < kLists; i++) if (static_cast<GmSocket*>(g_rl[i]) == p) return i;
    return p ? -2 : -1;
}
static int o_item(uint32_t p) {
    for (int i = 0; i < kItems; i++) if ((uint32_t)(uintptr_t)&g_oi[i] == p) return i;
    return p ? -2 : -1;
}
static int r_item(const void* p) {
    for (int i = 0; i < kItems; i++) if (static_cast<void*>(g_ri[i]) == p) return i;
    return p ? -2 : -1;
}

static void fail(const char* what, int o, int r) {
    if (g_failures++ < 15) std::printf("DIFF after op %llu: %s: original %d, recovered %d\n", g_ops, what, o, r);
}
static void eq(const char* what, int o, int r) {
    g_checks++;
    if (o != r) fail(what, o, r);
}

static void compare_graph() {
    eq("cursor", *(int*)(uintptr_t)kCursor, GmLinkedListNode::s_cursor);
    for (int i = 0; i < kLists; i++) {
        eq("list.head", o_node(g_ol[i].head), r_node(g_rl[i]->head));
        eq("list.tail", o_node(g_ol[i].tail), r_node(g_rl[i]->tail));
        eq("list.ctx", (int)(uintptr_t)g_ol[i].ctx, (int)(uintptr_t)g_rl[i]->m_context);
    }
    for (int i = 0; i < kItems; i++) eq("item.conn", o_node(g_oi[i].conn), r_conn(g_ri[i]->m_connections));
    for (int i = 0; i < kPoolSize; i++) {
        const ONode& o = opool()[i];
        const GmLinkedListNode& r = GmLinkedListNode::s_pool[i];
        eq("node.active", o.active, r.m_active);
        if (!o.active && !r.m_active) continue;  // a free entry's stale links are compared while it is in use only
        eq("node.pnext", o_node(o.pnext), r_conn(r.m_plugNext));
        eq("node.pprev", o_node(o.pprev), r_conn(r.m_plugPrev));
        eq("node.socket", o_list(o.socket), r_list(r.m_socket));
        eq("node.plug", o_item(o.plug), r_item(r.m_plug));
        eq("node.next", o_node(o.next), r_node(r.next));
        eq("node.prev", o_node(o.prev), r_node(r.prev));
    }
}

// Pseudo-random, reproducible.
static uint32_t g_rng = 1;
static uint32_t rnd(uint32_t n) {
    g_rng = g_rng * 1103515245u + 12345u;
    return ((g_rng >> 16) & 0x7FFF) % n;
}

static void o_list_ctor(int i) { at<Fn0>(0x00076DA0)(&g_ol[i], nullptr); }
static void o_item_ctor(int i) { at<Fn0>(0x00076C10)(&g_oi[i], nullptr); }

static void setup() {
    for (int i = 0; i < kPoolSize; i++) at<Fn0>(0x00076D20)(&opool()[i], nullptr);  // the pool's constructor
    for (int i = 0; i < kLists; i++) { o_list_ctor(i); g_rl[i] = new GmLinkedList; }
    for (int i = 0; i < kItems; i++) { o_item_ctor(i); g_ri[i] = new GmPlug; }
}

static void iterator_session(int li) {
    OIter o;
    at<Fn1>(0x00076E50)(&o, nullptr, &g_ol[li]);
    GmLinkedListIterator r(g_rl[li]);
    eq("iter.cur(ctor)", o_node(o.cur), r_node(r.m_cur));
    const int steps = 1 + rnd(12);
    for (int s = 0; s < steps; s++) {
        const int op = rnd(11);
        switch (op) {
            case 0: ((Fn0)slot(&o, 1))(&o, nullptr); r.First(); break;
            case 1: ((Fn0)slot(&o, 2))(&o, nullptr); r.Last(); break;
            case 2: ((Fn0)slot(&o, 3))(&o, nullptr); r.Next(); break;
            case 3:
                if (o.cur) { ((Fn0)slot(&o, 4))(&o, nullptr); r.Prev(); }  // both dereference a null current
                break;
            case 4:
                eq("iter.NextData", o_item((uint32_t)(uintptr_t)((FnP0)slot(&o, 5))(&o, nullptr)), r_item(r.NextData()));
                break;
            case 5:
                eq("iter.PrevData", o_item((uint32_t)(uintptr_t)((FnP0)slot(&o, 6))(&o, nullptr)), r_item(r.PrevData()));
                break;
            case 6:
                eq("iter.Current", o_item((uint32_t)(uintptr_t)((FnP0)slot(&o, 7))(&o, nullptr)), r_item(r.Current()));
                break;
            case 7:
                if (g_ol[li].head) {  // both dereference null on an empty list
                    const int index = (int)rnd(10) - 2;
                    eq("iter.At", o_item((uint32_t)(uintptr_t)((FnP1i)slot(&o, 8))(&o, nullptr, index)), r_item(r.At(index)));
                }
                break;
            case 8: eq("iter.Count", ((FnR0)slot(&o, 9))(&o, nullptr), r.Count()); break;
            case 9: at<Fn0>(0x00076F70)(&o, nullptr); r.RemoveCurrent(); break;
            case 10:
                if (rnd(4) == 0) { at<Fn0>(0x000770B0)(&o, nullptr); r.RemoveAll(); }
                break;
        }
        eq("iter.cur", o_node(o.cur), r_node(r.m_cur));
    }
    at<Fn0>(0x00137C80)(&o, nullptr);
    eq("iter.vptr(after dtor)", (int)(uintptr_t)o.vptr, 0x0027842C);
}

static void one_op() {
    const int li = rnd(kLists), ii = rnd(kItems);
    switch (rnd(12)) {
        case 0: case 1: case 2: case 3:
            ((Fn1)slot(&g_ol[li], 1))(&g_ol[li], nullptr, &g_oi[ii]);
            g_rl[li]->Append(g_ri[ii]);
            break;
        case 4: case 5:
            ((Fn1)slot(&g_ol[li], 2))(&g_ol[li], nullptr, &g_oi[ii]);
            if (!g_mutate) g_rl[li]->Remove(g_ri[ii]);
            break;
        case 6:
            eq("Contains", ((FnR1)slot(&g_ol[li], 3))(&g_ol[li], nullptr, &g_oi[ii]), g_rl[li]->Contains(g_ri[ii]));
            eq("IsEmpty", ((FnR0)slot(&g_ol[li], 4))(&g_ol[li], nullptr), g_rl[li]->IsEmpty());
            break;
        case 7: case 8:
            iterator_session(li);
            break;
        case 9:
            at<Fn0>(0x00076C20)(&g_oi[ii], nullptr);
            g_ri[ii]->DisconnectAll();
            break;
        case 10:  // destroy and rebuild an item: every connection must be severed
            at<Fn0>(0x00076C50)(&g_oi[ii], nullptr);
            o_item_ctor(ii);
            delete g_ri[ii];
            g_ri[ii] = new GmPlug;
            break;
        case 11:  // destroy and rebuild a list; a non-null context first, so that clearing it is observable
            at<Fn1>(0x000DF720)(&g_ol[li], nullptr, (void*)(uintptr_t)0x1234);
            g_rl[li]->SetContext((void*)(uintptr_t)0x1234);
            at<Fn0>(0x00076DD0)(&g_ol[li], nullptr);
            eq("vptr(~GmLinkedList)", (int)(uintptr_t)g_ol[li].vptr, 0x00278418);
            {
                // Destroy in place so the fields the destructor leaves behind can be compared before rebuilding.
                static_cast<GmLinkedList*>(g_rl[li])->~GmLinkedList();
                eq("ctx(after ~GmLinkedList)", (int)(uintptr_t)g_ol[li].ctx,
                   (int)(uintptr_t)reinterpret_cast<GmSocket*>(g_rl[li])->m_context);
            }
            o_list_ctor(li);
            new (g_rl[li]) GmLinkedList;
            break;
    }
    g_ops++;
    compare_graph();
}

// ---- focused scenarios -----------------------------------------------------------------------------------------------
static void pool_exhaustion() {
    // Empty both pools through the normal paths first, then run Allocate over the full range and the wrap-around.
    for (int i = 0; i < kItems; i++) { at<Fn0>(0x00076C20)(&g_oi[i], nullptr); g_ri[i]->DisconnectAll(); }
    compare_graph();
    ONode* o = opool();
    GmLinkedListNode* r = GmLinkedListNode::s_pool;
    *(int*)(uintptr_t)kCursor = kPoolSize - 3;
    GmLinkedListNode::s_cursor = kPoolSize - 3;
    for (int i = 0; i < kPoolSize + 5; i++) {
        const uint32_t a = (uint32_t)(uintptr_t)at<FnAlloc>(0x00076F90)();
        eq("Allocate", o_node(a), r_node(GmLinkedListNode::Allocate()));
        eq("cursor(alloc)", *(int*)(uintptr_t)kCursor, GmLinkedListNode::s_cursor);
    }
    for (int k = 0; k < 500; k++) {
        const int i = rnd(kPoolSize);
        o[i].active = 0;
        r[i].m_active = 0;
        const uint32_t a = (uint32_t)(uintptr_t)at<FnAlloc>(0x00076F90)();
        eq("Allocate(reuse)", o_node(a), r_node(GmLinkedListNode::Allocate()));
        eq("cursor(reuse)", *(int*)(uintptr_t)kCursor, GmLinkedListNode::s_cursor);
    }
    for (int i = 0; i < kPoolSize; i++) { o[i].active = 0; r[i].m_active = 0; }
}

static void connection_lifecycle() {
    // Node destructor and constructor directly: an in-use entry must leave both lists and reset its fields.
    ((Fn1)slot(&g_ol[0], 1))(&g_ol[0], nullptr, &g_oi[0]);
    g_rl[0]->Append(g_ri[0]);
    ((Fn1)slot(&g_ol[0], 1))(&g_ol[0], nullptr, &g_oi[1]);
    g_rl[0]->Append(g_ri[1]);
    compare_graph();
    const int o_first = o_node(g_ol[0].head), r_first = r_node(g_rl[0]->head);
    eq("first node", o_first, r_first);
    ONode* on = &opool()[o_first];
    GmLinkedListNode* rn = &GmLinkedListNode::s_pool[r_first];
    at<Fn0>(0x00076FF0)(on, nullptr);
    rn->~GmLinkedListNode();
    compare_graph();
    eq("vptr(~GmLinkedListNode)", (int)(uintptr_t)on->vptr, 0x002793D0);
    eq("node.socket(dtor)", (int)on->socket, (int)(uintptr_t)rn->m_socket);
    at<Fn0>(0x00076D20)(on, nullptr);
    new (rn) GmLinkedListNode;
    compare_graph();
    eq("vptr(GmLinkedListNode)", (int)(uintptr_t)on->vptr, 0x00278560);
    at<Fn0>(0x00076C20)(&g_oi[1], nullptr);
    g_ri[1]->DisconnectAll();
    compare_graph();
}

static void deleting_destructors() {
    // Slot 0 of each vftable, flags 0: destructor without freeing (the freeing path needs the game's heap: not run here).
    ((Fn1)slot(&g_ol[2], 1))(&g_ol[2], nullptr, &g_oi[3]);
    g_rl[2]->Append(g_ri[3]);
    compare_graph();
    eq("list sdd result", (int)(uintptr_t)((FnP1i)slot(&g_ol[2], 0))(&g_ol[2], nullptr, 0), (int)(uintptr_t)&g_ol[2]);
    eq("vptr(list sdd)", (int)(uintptr_t)g_ol[2].vptr, 0x00278418);
    o_list_ctor(2);
    delete g_rl[2];
    g_rl[2] = new GmLinkedList;
    compare_graph();

    ((Fn1)slot(&g_ol[2], 1))(&g_ol[2], nullptr, &g_oi[3]);
    g_rl[2]->Append(g_ri[3]);
    eq("plug sdd result", (int)(uintptr_t)((FnP1i)slot(&g_oi[3], 0))(&g_oi[3], nullptr, 0), (int)(uintptr_t)&g_oi[3]);
    o_item_ctor(3);
    delete g_ri[3];
    g_ri[3] = new GmPlug;
    compare_graph();

    ((Fn1)slot(&g_ol[2], 1))(&g_ol[2], nullptr, &g_oi[3]);
    g_rl[2]->Append(g_ri[3]);
    ONode* on = &opool()[o_node(g_ol[2].head)];
    GmLinkedListNode* rn = g_rl[2]->head;
    eq("node sdd result", (int)(uintptr_t)((FnP1i)slot(on, 0))(on, nullptr, 0), (int)(uintptr_t)on);
    rn->~GmLinkedListNode();
    compare_graph();
    at<Fn0>(0x00076D20)(on, nullptr);
    new (rn) GmLinkedListNode;
    compare_graph();

    // Connection base: constructor, deleting destructor. Abstract in the recovered world, so checked by field values.
    ONode raw;
    std::memset(&raw, 0xAB, sizeof raw);
    at<Fn0>(0x0007CE30)(&raw, nullptr);
    eq("connection vptr", (int)(uintptr_t)raw.vptr, 0x002793D0);
    eq("connection zeroed", raw.pnext | raw.pprev | raw.socket | raw.plug | (uint32_t)raw.active, 0);
    raw.pnext = raw.pprev = raw.socket = raw.plug = 0x1234;
    raw.active = 7;
    eq("connection sdd result", (int)(uintptr_t)((FnP1i)slot(&raw, 0))(&raw, nullptr, 0), (int)(uintptr_t)&raw);
    eq("connection zeroed(sdd)", raw.pnext | raw.pprev | raw.socket | raw.plug | (uint32_t)raw.active, 0);

    // Iterator deleting destructors: GmLinkedListIterator, then the abstract bases through their own vftables.
    OIter it;
    at<Fn1>(0x00076E50)(&it, nullptr, &g_ol[0]);
    eq("iterator sdd result", (int)(uintptr_t)((FnP1i)slot(&it, 0))(&it, nullptr, 0), (int)(uintptr_t)&it);
    eq("vptr(iterator sdd)", (int)(uintptr_t)it.vptr, 0x0027842C);
    at<Fn1>(0x00076020)(&it, nullptr, &g_ol[0]);
    eq("socket iterator vptr", (int)(uintptr_t)it.vptr, 0x00278454);
    eq("socket iterator socket", (int)it.socket, (int)(uintptr_t)&g_ol[0]);
    eq("socket iterator sdd result", (int)(uintptr_t)((FnP1i)slot(&it, 0))(&it, nullptr, 0), (int)(uintptr_t)&it);
    eq("vptr(socket iterator sdd)", (int)(uintptr_t)it.vptr, 0x0027842C);
    it.vptr = (void*)(uintptr_t)0x0027842C;
    eq("iterator base sdd result", (int)(uintptr_t)((FnP1i)slot(&it, 0))(&it, nullptr, 0), (int)(uintptr_t)&it);
    eq("vptr(iterator base sdd)", (int)(uintptr_t)it.vptr, 0x0027842C);
}

struct OOf { OList list; OItem plug; };

static void instantiation_destructor(bool through_plug_base) {
    // GmLinkedListOf<FaUiBase*>: the destructor (0x00097930) must sever from both bases. Second run enters through the
    // this-adjusting thunk in the GmPlug base's vftable (0x000979B0).
    class FaUiBase;
    static OOf o;
    at<Fn0>(0x00076DA0)(&o.list, nullptr);
    at<Fn0>(0x00076C10)(&o.plug, nullptr);
    o.list.vptr = (void*)(uintptr_t)0x0027BCA4;
    o.plug.vptr = (void*)(uintptr_t)0x0027BCBC;
    ((Fn1)slot(&o, 1))(&o, nullptr, &g_oi[2]);                // list holds an item
    ((Fn1)slot(&g_ol[1], 1))(&g_ol[1], nullptr, &o.plug);     // and is itself held by another list
    auto* r = new GmLinkedListOf<FaUiBase*>;
    r->Append(g_ri[2]);
    g_rl[1]->Append(static_cast<GmPlug*>(r));
    eq("of.conn", o_node(o.plug.conn), r_conn(static_cast<GmPlug*>(r)->m_connections));
    eq("of.head", o_node(o.list.head), r_node(r->head));
    // Scalar deleting destructor, no free (flags 0).
    if (through_plug_base) ((FnP1i)slot(&o.plug, 0))(&o.plug, nullptr, 0);
    else ((FnP1i)slot(&o, 0))(&o, nullptr, 0);
    delete r;
    eq("of.head(after)", o.list.head != 0, 0);
    eq("item2.conn", o_node(g_oi[2].conn), r_conn(g_ri[2]->m_connections));
    eq("list1.head", o_node(g_ol[1].head), r_node(g_rl[1]->head));
    compare_graph();
}

int main(int argc, char** argv) {
    const char* path = nullptr;
    const char* entries = nullptr;
    unsigned seed = 1;
    unsigned long long ops = 20000;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoul(argv[++i], nullptr, 0);
        else if (!std::strcmp(argv[i], "--ops") && i + 1 < argc) ops = std::strtoull(argv[++i], nullptr, 0);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else path = argv[i];
    }
    if (!path) { std::printf("usage: oracle_gm <default.xbe> [--seed N] [--ops N] [--mutate]\n"); return 2; }
    if (!map_image(path)) return 2;
    if (entries && !arm_entries(entries)) return 2;
    g_rng = seed;
    setup();
    compare_graph();
    for (unsigned long long i = 0; i < ops; i++) one_op();
    connection_lifecycle();
    pool_exhaustion();
    deleting_destructors();
    instantiation_destructor(false);
    instantiation_destructor(true);
    report_entries();
    std::printf("%s: seed %u, %llu random operations, %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", seed, g_ops, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
