// Part of tests/oracle/oracle_smallvm.cpp: the script system's loading, starting, running and stopping, the owner, the factory and the instance.
#pragma once

static const std::vector<unsigned char>* g_slot_file[AtSmallCSystem::kMaxScripts];
static const std::vector<unsigned char>* find_file(const char* name) {
    for (const Script& s : g_scripts) {
        if (_strnicmp(s.name.c_str(), name, std::strlen(name)) == 0 && s.name.size() > std::strlen(name) && s.name[std::strlen(name)] == '.') return &s.data;
    }
    return nullptr;
}
static int prog_extent(const unsigned char* p) {
    const AtAmxHeader* h = (const AtAmxHeader*)p;
    return h->m_stp + 4 > h->m_size ? h->m_stp + 4 : h->m_size;
}
static void system_regions() {
    clear_regions();
    for (int side = 0; side < 2; side++) {
        add_region(side, side == 0 ? g_ofactory : g_rfactory_mem, 16, 0x30000000);
        AtSmallCSystem* s = sys(side);
        for (int i = 0; i < AtSmallCSystem::kMaxScripts; i++) {
            AtSmallCScript* c = (AtSmallCScript*)s->m_scripts[i];
            if (!c) continue;
            add_region(side, c, sizeof(AtSmallCScript), 0x40000000 + i * 0x100);
            if (c->m_program && c->m_shared == 0) add_region(side, c->m_program, (uint32_t)prog_extent(c->m_program), 0x50000000 + i * 0x10000);
        }
        // A shared program whose owner was stopped is freed under the scripts still using it (as in the original): its pointers are compared by offset,
        // never read.
        for (int i = 0; i < AtSmallCSystem::kMaxScripts; i++) {
            AtSmallCScript* c = (AtSmallCScript*)s->m_scripts[i];
            if (c && c->m_program && c->m_shared != 0 && rel_on(side, c->m_program) == (uint32_t)(uintptr_t)c->m_program) {
                add_region(side, c->m_program, 0x10000, 0x70000000 + i * 0x10000);
            }
        }
    }
}
static bool program_live(int side, const AtSmallCScript* c) {
    const uint32_t r = rel_on(side, c->m_program);
    return (r & 0xF0000000) == 0x50000000;
}
static const char* const kNames[] = {"BKID", "KID2", "SH", "BAISHELD", "BAIPATHH", "PATH", "X", "BAD", "NOPE", "W1", "W2", "DIRKID", "ABCDEFGH", "SKID", ""};
static void* o_find(void* map, const char* name) { return at<ONameP>(0x00025150)(map, nullptr, name); }

static void compare_systems(const char* what) {
    compare_logs(what);
    system_regions();
    eq(F(what, "count"), g_osys->m_scriptCount, g_rsys->m_scriptCount);
    eq(F(what, "last"), g_osys->m_lastStarted, g_rsys->m_lastStarted);
    eq(F(what, "natives"), g_osys->m_nativeCount, g_rsys->m_nativeCount);
    for (int i = 0; i < AtSmallCSystem::kMaxNatives; i++) {
        eq(F(what, "native name"), std::memcmp(g_osys->m_natives[i].m_name, g_rsys->m_natives[i].m_name, 0x14), 0);
        const bool game = (void*)g_rsys->m_natives[i].m_function == (void*)d_native || g_rsys->m_natives[i].m_function == nullptr;
        if (game) eq(F(what, "native"), (uint32_t)(uintptr_t)g_osys->m_natives[i].m_function, (uint32_t)(uintptr_t)g_rsys->m_natives[i].m_function);
        else eq(F(what, "core native"), (uint32_t)(uintptr_t)g_osys->m_natives[i].m_function >= 0x0007C800, 1);
    }
    eq(F(what, "host variables"), std::memcmp(g_osys->m_hostVars, g_rsys->m_hostVars, sizeof g_osys->m_hostVars), 0);
    for (int i = 0; i < AtSmallCSystem::kMaxScripts; i++) {
        AtSmallCScript* o = (AtSmallCScript*)g_osys->m_scripts[i];
        AtSmallCScript* r = (AtSmallCScript*)g_rsys->m_scripts[i];
        eq(F(what, "slot"), o != nullptr, r != nullptr);
        if (!o || !r) continue;
        compare_script(F(what, "script"), *o, *r);
        if (g_slot_file[i] && o->m_program && r->m_program && program_live(0, o) && program_live(1, r)) compare_memory(F(what, "program"), o->m_program, r->m_program, *g_slot_file[i], prog_extent(o->m_program));
    }
    for (const char* n : kNames) eq(F(what, n), rel_on(0, o_find(&g_osys->m_names, n)), rel_on(1, g_rsys->m_names.Find(n)));
    eq(F(what, "step"), g_osys->m_stepMode, g_rsys->m_stepMode);
    compare_globals(what);
}

static void serve(const std::vector<unsigned char>* file) {
    for (int side = 0; side < 2; side++) {
        g_fs->files[side].data = file ? file->data() : nullptr;
        g_fs->files[side].size = file ? file->size() : 0;
        g_fs->files[side].mode = kNormal;
    }
}

// Runs one call on each side, each under its own log; crashes compared.
template <class O, class R> static int both(const char* what, O o, R r) {
    int v[2] = {0, 0};
    bool crashed[2];
    const uint32_t seed = g_answer[0];
    int vars[2][8];
    std::memcpy(vars[0], g_hostvar, sizeof g_hostvar);
    for (int side = 0; side < 2; side++) {
        if (side == 1) {  // one set of host variables serves both sides: the first side's writes are kept aside and undone
            std::memcpy(vars[1], g_hostvar, sizeof g_hostvar);
            std::memcpy(g_hostvar, vars[0], sizeof g_hostvar);
        }
        g_answer[side] = seed;
        On on(side);
        crashed[side] = guarded([&] { v[side] = side == 0 ? o() : r(); });
    }
    eq(F(what, "crashed"), crashed[0], crashed[1]);
    eq(F(what, "host variables"), std::memcmp(vars[1], g_hostvar, sizeof g_hostvar), 0);
    if (!crashed[0]) eq(F(what, "result"), v[0], v[1]);
    eq(F(what, "answers"), g_answer[0], g_answer[1]);
    return v[0];
}

static AtSmallCScriptDesc g_desc;
static int start_both(const char* what, const char* file, const char* name, int enabled, const char* entry, int flag40, int shared,
                      const std::vector<unsigned char>* served) {
    std::memset(&g_desc, 0xCC, sizeof g_desc);
    std::memset(g_desc.m_file, 0, 8);
    std::memset(g_desc.m_name, 0, 8);
    std::memcpy(g_desc.m_file, file, std::strlen(file) < 8 ? std::strlen(file) : 8);
    std::memcpy(g_desc.m_name, name, std::strlen(name) < 8 ? std::strlen(name) : 8);
    g_desc.m_enabled = enabled;
    strcpy_s(g_desc.m_entry, entry);
    g_desc.m_flag40 = flag40;
    g_desc.m_shared = shared;
    g_context = std::string("start ") + what;
    serve(served);
    const int before = g_osys->m_scriptCount;
    const int r = both(what, [] { return at<OStart>(0x00075CB0)(g_osys, nullptr, &g_desc); }, [] { return g_rsys->StartScript(&g_desc); });
    if (g_osys->m_scriptCount != before) g_slot_file[g_osys->m_lastStarted] = served;
    compare_systems(what);
    return r;
}

static AtSmallCScript* slot_of_name(int side, const char* name) {
    return (AtSmallCScript*)(side == 0 ? o_find(&g_osys->m_names, name) : g_rsys->m_names.Find(name));
}

static void call_public(const char* name, int index, int copy) {
    AtSmallCScript* o = slot_of_name(0, name);
    AtSmallCScript* r = slot_of_name(1, name);
    if (!o || !r) return;
    g_context = std::string("call ") + name + " " + std::to_string(index) + (copy ? " on a copy" : "");
    both("call public", [&] { return at<OCall>(0x00075B20)(g_osys, nullptr, o, index, copy); }, [&] { return g_rsys->CallPublic(r, index, copy); });
    compare_systems("call public");
}

static void run_direct(const char* name) {
    AtSmallCScript* o = slot_of_name(0, name);
    AtSmallCScript* r = slot_of_name(1, name);
    if (!o || !r) return;
    const int n = (int)(rnd() % 4);
    int params[3];
    for (int& v : params) v = rv();
    g_context = std::string("run ") + name;
    both("run", [&] { return at<ORun>(0x00075860)(g_osys, o, n, params[0], params[1], params[2]); }, [&] { return AtSmallCSystem::Run(g_rsys, r, n, params); });
    compare_systems("run");
}

static int name_both(const char* what, uint32_t va, int (AtSmallCSystem::*fn)(const char*), const char* name) {
    g_context = std::string(what) + " " + name;
    const int v = both(what, [&] { return at<OName>(va)(g_osys, nullptr, name); }, [&] { return (g_rsys->*fn)(name); });
    compare_systems(what);
    return v;
}

static void run_loader() {
    // LoadProgram alone, over every file and the ways a file can fail.
    std::vector<std::vector<unsigned char>> files;
    for (const Script& s : g_scripts) files.push_back(s.data);
    {
        Prog p = make_program({ADD, SYSREQ_C});
        files.push_back(p.build());
        std::vector<unsigned char> f = p.build();
        ((AtAmxHeader*)f.data())->m_magic = 0;  // Init refuses: the program is freed
        files.push_back(f);
        f = p.build();
        ((AtAmxHeader*)f.data())->m_stp = 0;  // refused before any allocation
        files.push_back(f);
        f = p.build();
        ((AtAmxHeader*)f.data())->m_stp = -4;
        files.push_back(f);
        p.compact = 1;
        files.push_back(p.build());
        files.push_back(std::vector<unsigned char>());  // empty: the header's read fails
    }
    static const char* const names[] = {"BKID", "BAISHELDX", "A", "LONGNAMES"};
    for (size_t k = 0; k < files.size() * 5; k++) {
        const std::vector<unsigned char>& f = files[k % files.size()];
        const int variant = (int)(k / files.size());
        serve(&f);
        g_fs->has = variant != 3;
        g_fs->open_status = variant == 4 ? 2 : 0;
        if (variant == 2) for (int side = 0; side < 2; side++) g_fs->files[side].mode = kPendingIo;
        g_osys->m_programFs = g_rsys->m_programFs = variant == 1 ? g_fs : nullptr;
        const char* name = names[k % 4];
        g_context = "load " + std::to_string(k);
        static unsigned char amx[2][sizeof(AtAmx)];
        std::memset(amx, 0xEE, sizeof amx);
        void* got[2];
        for (int side = 0; side < 2; side++) {
            On on(side);
            got[side] = side == 0 ? at<OLoad>(0x000758B0)(g_osys, nullptr, amx[0], name, 5) : g_rsys->LoadProgram((AtAmx*)amx[1], name, 5);
        }
        clear_regions();
        for (int side = 0; side < 2; side++) if (got[side]) add_region(side, got[side], (uint32_t)prog_extent((unsigned char*)got[side]), 0x60000000);
        compare_logs("load");
        eq("load result", got[0] != nullptr, got[1] != nullptr);
        if (got[0] && got[1]) {
            compare_amx("load amx", 0, *(AtAmx*)amx[0], *(AtAmx*)amx[1]);
            compare_memory("load memory", (unsigned char*)got[0], (unsigned char*)got[1], f, prog_extent((unsigned char*)got[0]));
        } else {
            eq("load amx", std::memcmp(amx[0], amx[1], sizeof(AtAmx)), 0);
        }
        compare_globals("load");
        for (int side = 0; side < 2; side++) std::oracle_free(got[side]);
    }
    g_fs->has = true;
    g_fs->open_status = 0;
    g_osys->m_programFs = g_rsys->m_programFs = nullptr;
}

static void owner_cases(const std::vector<unsigned char>* w3) {
    static const char* const paths[] = {"Design\\Behavior\\BKID", "BAIPATHH", "Design\\Behavior\\ABCDEFGHIJ", "A\\B\\C\\BAISHELD", "\\SKID"};
    for (const char* path : paths) {
        const char* leaf = std::strrchr(path, '\\') ? std::strrchr(path, '\\') + 1 : path;
        const std::vector<unsigned char>* served = find_file(leaf) ? find_file(leaf) : w3;
        serve(served);
        g_context = std::string("owner load ") + path;
        GmScript* got[2];
        const int before = g_osys->m_scriptCount;
        for (int side = 0; side < 2; side++) {
            On on(side);
            got[side] = side == 0 ? (GmScript*)at<OOwnerLoad>(0x00070310)(g_oowner, nullptr, path) : ((GmScriptOwner*)g_rowner)->Load(path);
        }
        if (g_osys->m_scriptCount != before) g_slot_file[g_osys->m_lastStarted] = served;
        compare_systems("owner load");
        eq("owner load result", rel_on(0, got[0]), rel_on(1, got[1]));
        if (got[0] && got[1]) {
            for (int i = 0; i < 4; i++) {
                g_context = std::string("owner call ") + path;
                both("owner call", [&] { return at<OOwnerCall>(0x00070190)(g_oowner, nullptr, got[0], i); },
                     [&] { return ((GmScriptOwner*)g_rowner)->CallPublic(got[1], i); });
                compare_systems("owner call");
            }
        }
        g_context = std::string("owner start ") + path;
        void* st[2];
        const int before2 = g_osys->m_scriptCount;
        for (int side = 0; side < 2; side++) {
            On on(side);
            st[side] = side == 0 ? at<OOwnerStart>(0x00070220)(g_oowner, nullptr, "Somewhere\\Else", "DIRKID")
                                 : ((GmScriptOwner*)g_rowner)->Start("Somewhere\\Else", "DIRKID");
        }
        if (g_osys->m_scriptCount != before2) g_slot_file[g_osys->m_lastStarted] = served;
        compare_systems("owner start");
        eq("owner start result", rel_on(0, st[0]), rel_on(1, st[1]));
        g_context = std::string("owner release ") + path;
        for (int side = 0; side < 2; side++) {
            On on(side);
            if (side == 0) at<ORelease>(0x00070150)(g_oowner, nullptr, got[0]);
            else ((GmScriptOwner*)g_rowner)->Release(got[1]);
        }
        for (int side = 0; side < 2; side++) {
            On on(side);
            if (side == 0) at<ORelease>(0x00070150)(g_oowner, nullptr, nullptr);
            else ((GmScriptOwner*)g_rowner)->Release(nullptr);
        }
        compare_systems("owner release");
        name_both("stop DIRKID", 0x00075660, &AtSmallCSystem::StopScript, "DIRKID");
    }
}

static void factory_cases() {
    for (int k = 0; k < 6; k++) {
        g_context = "factory " + std::to_string(k);
        AtSmallCScript* made[2];
        for (int side = 0; side < 2; side++) {
            On on(side);
            made[side] = side == 0 ? (AtSmallCScript*)at<OCreate>(0x0006F5A0)(g_ofactory, nullptr) : ((AtSmallCScriptFactory*)g_rfactory_mem)->Create();
        }
        clear_regions();
        for (int side = 0; side < 2; side++) {
            add_region(side, side == 0 ? g_ofactory : g_rfactory_mem, 16, 0x30000000);
            add_region(side, made[side], sizeof(AtSmallCScript), 0x40000000);
        }
        compare_logs("create");
        compare_script("created", *made[0], *made[1]);
        for (int side = 0; side < 2; side++) {
            if (k % 3 == 1) {
                made[side]->m_program = (unsigned char*)operator new(64);  // freed by the release unless shared
                made[side]->m_shared = k % 2;
            }
            On on(side);
            if (side == 0) at<ORelease>(0x0006F640)(g_ofactory, nullptr, made[0]);
            else ((AtSmallCScriptFactory*)g_rfactory_mem)->Release(made[1]);
        }
        compare_logs("release");
    }
    g_context = "release null";
    bool crashed[2];
    for (int side = 0; side < 2; side++) {
        On on(side);
        crashed[side] = guarded([&] {
            if (side == 0) at<ORelease>(0x0006F640)(g_ofactory, nullptr, nullptr);
            else ((AtSmallCScriptFactory*)g_rfactory_mem)->Release(nullptr);
        });
    }
    eq("release null crashed", crashed[0], crashed[1]);
    compare_logs("release null");
}

static void instance_cases() {
    g_context = "instance";
    *(void**)(uintptr_t)0x002CF174 = nullptr;
    AtSmallCSystem::s_instance = nullptr;
    void* made[2];
    for (int side = 0; side < 2; side++) {
        On on(side);
        made[side] = side == 0 ? at<OInstance>(0x0007AF10)() : AtSmallCSystem::Instance();
    }
    compare_logs("instance");
    const AtSmallCSystem* o = (const AtSmallCSystem*)made[0];
    const AtSmallCSystem* r = (const AtSmallCSystem*)made[1];
    eq("instance made", o != nullptr, r != nullptr);
    if (o && r) {
        eq("instance count", o->m_scriptCount, r->m_scriptCount);
        eq("instance running", o->m_running, r->m_running);
        eq("instance 5B3C", o->m_unknown5B3C, r->m_unknown5B3C);
        eq("instance natives", o->m_nativeCount, r->m_nativeCount);
        eq("instance heap", *(int*)(uintptr_t)0x002CF170, (int)(uintptr_t)AtSmallCSystem::s_heap);
    }
    void* again[2];
    for (int side = 0; side < 2; side++) {
        On on(side);
        again[side] = side == 0 ? at<OInstance>(0x0007AF10)() : AtSmallCSystem::Instance();
    }
    compare_logs("instance again");
    eq("instance kept", again[0] == made[0], again[1] == made[1]);
    *(AtSmallCSystem**)(uintptr_t)0x002CF174 = g_osys;
    AtSmallCSystem::s_instance = g_rsys;
    *(void**)(uintptr_t)0x002CF170 = &g_heap_token;  // the constructors clear it
    AtSmallCSystem::s_heap = &g_heap_token;
}

// The machine's API on its own: FindPublic, Register with limited and changed lists, RegisterHostVars missing a name, the name compare.
static void run_api() {
    for (const Script& sc : g_scripts) {
        Pair p;
        g_context = "api " + sc.name;
        prepare(p, sc.data, false, -9);
        const AtAmxHeader* h = (const AtAmxHeader*)sc.data.data();
        std::vector<std::string> names = {"", "a", "func", "func0000", "func00000", "func0001x", "func9999", "zzz", "FUNC0001", "func0002a"};
        for (int i = 0; i < h->m_numPublics; i++) names.push_back(((const AtAmxFuncStub*)(sc.data.data() + h->m_publics))[i].m_name);
        for (const std::string& n : names) {
            int index[2] = {-5, -5};
            const int r = both("find public", [&] { return at<OFindPublic>(0x0007AC20)(&p.s[0]->m_amx, n.c_str(), &index[0]); },
                               [&] { return AtAmxFindPublic(&p.s[1]->m_amx, n.c_str(), &index[1]); });
            (void)r;
            eq("find public index", index[0], index[1]);
        }
        // GetAddr over every region: data, the heap gap, the stack, past its end, before the data.
        for (int a = -0x40; a < p.s[0]->m_amx.m_stp + 0x40; a += 4 + (a > 0 && a < p.s[0]->m_amx.m_stp - 0x100 ? 0x3C : 0)) {
            int* phys[2] = {nullptr, nullptr};
            const int r = both("get addr", [&] { return at<OGetAddr>(0x0007AFD0)(&p.s[0]->m_amx, a, &phys[0]); }, [&] { return AtAmxGetAddr(&p.s[1]->m_amx, a, &phys[1]); });
            if (r == 0) eq("get addr pointer", rel_on(0, phys[0]), rel_on(1, phys[1]));
        }
        // Register with a list cut short, so the count decides; then again with other contexts (registered entries are kept); a null context.
        static AtSmallCNativeEntry list[64];
        for (int round = 0; round < 6; round++) {
            if (round == 4) {
                release(p);
                prepare(p, sc.data, false, -9);
            }
            std::memcpy(list, g_list, sizeof list);
            if (round >= 3) for (int i = 0; i < kNativeCount; i++) list[i].m_context = g_tokens[(i + round) % 60];
            if (round == 4) for (int i = 0; i < kNativeCount; i += 2) list[i].m_context = nullptr;  // registered all the same, with error 19
            const int count = round == 0 ? 2 : (round == 1 ? (int)(rnd() % kNativeCount) : (round == 2 ? kNativeCount : -1));
            both("register", [&] { return at<ORegister>(0x0007ADD0)(&p.s[0]->m_amx, list, count); }, [&] { return AtAmxRegister(&p.s[1]->m_amx, list, count); });
            compare_pair("register", p, sc.data);
        }
        release(p);
    }
    // A program declaring a host variable the list does not have, with a count that ends the search.
    {
        Prog q = make_program({ADD});
        q.hostvars.push_back("hvZ");
        const std::vector<unsigned char> f = q.build();
        Pair p;
        g_context = "api host variables";
        prepare(p, f, false, -1);
        for (int count : {3, 2, 0, 1}) {
            both("host variables", [&] { return at<int(__cdecl*)(void*, const void*, int)>(0x0007B160)(&p.s[0]->m_amx, g_hostvars, count); },
                 [&] { return AtAmxRegisterHostVars(&p.s[1]->m_amx, g_hostvars, count); });
            compare_pair("host variables", p, f);
        }
        release(p);
    }
    static const char* const pairs[][2] = {{"ABCDEFGH", "ABCDEFGX"}, {"ABCDEFGH", "ABCDEFGHX"}, {"A", "B"}, {"B", "A"}, {"\x80", "A"}, {"A", "\xF0"}, {"", "A"},
                                          {"abc", "abc"}, {"ABCDEFG", "ABCDEFGH"}, {nullptr, ""}, {"X", nullptr}, {nullptr, nullptr}};
    for (const auto& pr : pairs) {
        std::string a = pr[0] ? pr[0] : "", b = pr[1] ? pr[1] : "";
        const char* x = pr[0] ? a.c_str() : nullptr;
        const char* y = pr[1] ? b.c_str() : nullptr;
        g_context = "compare names";
        both("compare names", [&] { return at<OCompare>(0x00013F00)(x, y); }, [&] { return AtCompareNames8(x, y); });
        compare_logs("compare names");
    }
}

static void run_system() {
    run_api();
    run_loader();
    g_answer_mode = 2;
    static std::vector<unsigned char> w1, w2, w3;
    {
        Prog p = make_program({SUSPEND, SYSREQ_C, ADD});
        w1 = p.build();
        Prog q = make_program({HALT});
        q.natives.push_back("nD");
        w2 = q.build();
        Prog r = make_program({SDIV});
        w3 = r.build();
    }
    const std::vector<unsigned char>* kid = find_file("BKID");
    const std::vector<unsigned char>* shield = find_file("BAISHELD");
    start_both("BKID", "BKID", "BKID", 0, "", 0, 0, kid);
    start_both("BKID again", "BKID", "BKID", 1, "", 0, 0, kid);
    start_both("shared", "BKID", "KID2", 1, "func0003", 1, 5, kid);
    start_both("shared, no source", "NOPE", "X", 0, "", 0, 7, kid);
    start_both("absent entry", "BAISHELD", "SH", 0, "nosuch", 0, 0, shield);
    start_both("entry", "BAISHELD", "BAISHELD", 1, "func0002", 0, 0, shield);
    g_fs->open_status = 2;
    start_both("cannot open", "BAD", "BAD", 0, "", 0, 0, &w3);
    g_fs->open_status = 0;
    start_both("written", "W1", "W1", 0, "beta", 0, 0, &w1);
    // A built-in missing: Register warns into a block it then frees from the middle.
    name_both("find native", 0x00075530, &AtSmallCSystem::FindNative, "nD");
    name_both("unregister", 0x00075760, &AtSmallCSystem::Unregister, "nD");
    name_both("unregister absent", 0x00075760, &AtSmallCSystem::Unregister, "nD");
    name_both("find native absent", 0x00075530, &AtSmallCSystem::FindNative, "nD");
    start_both("missing built-in", "W2", "W2", 0, "", 0, 0, &w2);
    at<OReg>(0x000754D0)(g_osys, nullptr, g_tokens[kNativeCount - 1], "nD", (AtSmallCNative)d_native);
    g_rsys->Register(g_tokens[kNativeCount - 1], "nD", (AtSmallCNative)d_native);
    start_both("built-in back", "W2", "W2", 0, "", 0, 0, &w2);
    const int count = g_osys->m_scriptCount;
    g_osys->m_scriptCount = g_rsys->m_scriptCount = AtSmallCSystem::kMaxScripts;
    start_both("full", "W2", "FULL", 0, "", 0, 0, &w2);
    g_osys->m_scriptCount = g_rsys->m_scriptCount = count;
    start_both("path", "BAIPATHH", "PATH", 0, "", 0, 0, find_file("BAIPATHH"));
    start_both("eight", "BAISHELD", "ABCDEFGX", 0, "", 0, 0, shield);
    name_both("index eight", 0x00075600, &AtSmallCSystem::FindScriptIndex, "ABCDEFGH");
    name_both("index eight", 0x00075600, &AtSmallCSystem::FindScriptIndex, "ABCDEFGX");
    name_both("stop eight", 0x00075660, &AtSmallCSystem::StopScript, "ABCDEFGH");
    name_both("find first native", 0x00075530, &AtSmallCSystem::FindNative, "numargs");

    // Calls, by every way in, with every answer mode.
    for (int mode = 0; mode < 4; mode++) {
        g_answer_mode = mode;
        for (const char* n : {"BKID", "KID2", "SH", "BAISHELD", "PATH", "W1", "W2", "NOPE"}) {
            for (int i = -1; i < 8; i++) {
                call_public(n, i, (int)(rnd() % 2));
                if (rnd() % 3 == 0) run_direct(n);
            }
        }
    }
    // A suspended call: activate refuses, cancel abandons it, run resumes it.
    for (int k = 0; k < 6; k++) {
        call_public("W1", -1, 0);
        name_both("activate", 0x00075A90, &AtSmallCSystem::Activate, "W1");
        if (k % 2) name_both("cancel", 0x00075AE0, &AtSmallCSystem::Cancel, "W1");
        run_direct("W1");
        name_both("cancel idle", 0x00075AE0, &AtSmallCSystem::Cancel, "W1");
        name_both("activate idle", 0x00075A90, &AtSmallCSystem::Activate, "W2");
    }
    g_answer_mode = 0;  // answers that raise no error: the call finishes
    call_public("BKID", 1, 0);
    name_both("activate finished", 0x00075A90, &AtSmallCSystem::Activate, "BKID");
    name_both("cancel finished", 0x00075AE0, &AtSmallCSystem::Cancel, "BKID");
    name_both("activate absent", 0x00075A90, &AtSmallCSystem::Activate, "NOPE");
    name_both("cancel absent", 0x00075AE0, &AtSmallCSystem::Cancel, "NOPE");
    AtSmallCScript* sh[2] = {slot_of_name(0, "SH"), slot_of_name(1, "SH")};
    if (sh[0] && sh[1]) {
        sh[0]->m_valid = sh[1]->m_valid = 0;
        name_both("activate invalid", 0x00075A90, &AtSmallCSystem::Activate, "SH");
        name_both("cancel invalid", 0x00075AE0, &AtSmallCSystem::Cancel, "SH");
        sh[0]->m_valid = sh[1]->m_valid = 1;
    }
    for (const char* n : kNames) {
        name_both("index", 0x00075600, &AtSmallCSystem::FindScriptIndex, n);
        g_context = std::string("find ") + n;
        void* f[2];
        for (int side = 0; side < 2; side++) {
            On on(side);
            f[side] = side == 0 ? at<ONameP>(0x00075650)(g_osys, nullptr, n) : (void*)g_rsys->FindScript(n);
        }
        system_regions();
        eq("find script", rel_on(0, f[0]), rel_on(1, f[1]));
        compare_logs("find script");
    }
    g_osys->HostVars()[2].m_address = g_rsys->HostVars()[2].m_address = nullptr;
    call_public("W1", 1, 1);
    g_osys->HostVars()[2].m_address = g_rsys->HostVars()[2].m_address = &g_hostvar[2];
    owner_cases(&w3);
    for (const char* n : kNames) name_both("stop", 0x00075660, &AtSmallCSystem::StopScript, n);
    for (const char* n : kNames) name_both("stop again", 0x00075660, &AtSmallCSystem::StopScript, n);
    factory_cases();
    instance_cases();
}
