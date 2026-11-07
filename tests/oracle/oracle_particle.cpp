// Differential oracle for ParticleSystemActor and the table of random words (Windows x86, local only). See docs/verification.md.
//
//   oracle_particle <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Built on oracle_asn.cpp, whose harness it shares. Every .ASN file is loaded, cleared and destroyed on both sides with the real class. The emitters and the drawers are stand-ins on both sides: real ones come from .ASE files and from the running
// game. Their shapes, 0 to 4 echoes and 1 to 7 particles, follow from each name. The table itself is compared with the owner's XBE, word by word.
#define ORACLE_ACTOR
#include "oracle_asn.cpp"

static std::string particle(const std::string& header, const std::string& body) {
    return std::string(kHead) + "ParticleSystemActor[1]\n{\n ParticleSystemActor \"Self\" " + header + "\n {\n" + body + "\n }\n}\n";
}

static void run_particle_table() {
    for (unsigned i = 0; i < 0x800; i++) eq("random word", *(const uint16_t*)(uintptr_t)(0x002AF0B0 + i * 2), AtRandomWord(i));
    // The cursor wraps, from each place it could be. The original's draws are inline in 0x0004C3B0, which runs in the cases below; this is the recovered side's, against the same formula as written in the original.
    for (unsigned from : {0u, 1u, 0x7FEu, 0x7FFu}) {
        g_atRandomCursor = from;
        const unsigned word = AtNextRandomWord();
        const unsigned next = (from + 1) & 0x7FF;
        eq("next word", *(const uint16_t*)(uintptr_t)(0x002AF0B0 + next * 2), word);
        eq("cursor moved", next, g_atRandomCursor);
    }
    g_atRandomCursor = 0;
    *(uint32_t*)(uintptr_t)0x002C60C0 = 0;
}

static void run_particle_words() {
    // Words of its own, and how each shows. Self has one shape; the others are chosen to cover echoes and particles.
    static const char* const bodies[] = {
        "Emitter \"Self\" \"\"", "Emitter \"Self\" \"Space\"", "Emitter \"Puff\" \"\"", "Emitter \"Puff\" \"Space\"", "Emitter \"Self\" \"\"\nEmitter \"Puff\" \"\"", "Emitter \"Puff\" \"\"\nEmitter \"XGone\" \"\"",
        "Emitter \"XGone\" \"\"", "Emitter \"Self\" \"XNone\"", "Emitter \"Self\"", "Emitter", "Emitter 5 5",
        "",  // No emitter at all: the report, and then a crash through the null, on both sides.
        "DrawingDisable", "BoundingSphere 1.5 2.5 3.5 4.5",

        "Emitter \"Self\" \"\"\nWorldAdditiveInitialVelocity 1.5 2.5 3.5", "Emitter \"Self\" \"\"\nWorldAdditiveInitialVelocity 1.5 2.5", "Emitter \"Self\" \"\"\nWorldAdditiveInitialVelocity 1.5 2.5 3.5\nWorldAdditiveInitialVelocity -7.25 8.125 0.375",
        "Emitter \"Self\" \"\"\nWorldAdditiveAcceleration 1.5 2.5 3.5", "Emitter \"Self\" \"\"\nWorldAdditiveAcceleration 0.3 -981.7 1.0e7", "Emitter \"Self\" \"\"\nWorldAdditiveAcceleration 1.5 2.5", "Emitter \"Self\" \"\"\nWorldAdditiveAcceleration 1.5 2.5 3.5\nWorldAdditiveAcceleration -7.25 8.125 0.375",
        "WorldAdditiveAcceleration 1.5 2.5 3.5\nEmitter \"Self\" \"\"\nWorldAdditiveInitialVelocity 4.5 5.5 6.5",

        "Emitter \"Self\" \"\"\nUpdateAtDraw", "Emitter \"Self\" \"\"\nPlayParticles", "Emitter \"Self\" \"\"\nPauseParticles", "Emitter \"Self\" \"\"\nPauseParticles\nPlayParticles", "Emitter \"Self\" \"\"\nPlayParticles\nPauseParticles", "Emitter \"Self\" \"\"\nUpdateAtDraw\nPauseParticles",

        // What is not its own goes on to the base, and past the range the original's table covers, on each side of it.
        "Emitter \"Self\" \"\"\nStartTime 1.5", "Emitter \"Self\" \"\"\nPlaySegment", "Emitter \"Self\" \"\"\nMorphEnable", "Emitter \"Self\" \"\"\nMatAnimationChannel[0]\n{\n}", "Emitter \"Self\" \"\"\nDrawingDisable\nDrawingEnable", "Emitter \"Self\" \"\"\nMesh \"Self\" \"\"",
        "Emitter \"Self\" \"\"\nOther", "Emitter \"Self\" \"\"\n5", "Emitter \"Self\" \"\"\nMatAssignment \"Self\" \"\"", "Emitter \"Self\" \"\"\nModifierReceiver\nProjectionReceiver",
        // A sphere said in the file is lost to the emitter's.
        "BoundingSphere 1.5 2.5 3.5 4.5\nEmitter \"Self\" \"\"", "Emitter \"Self\" \"\"\nBoundingSphere 1.5 2.5 3.5 4.5",
    };
    for (const char* b : bodies) {
        for (const char* h : {"RotTransCoordSys MotionState", "RotTransCoordSys", ""}) {
            const std::string t = particle(h, b);
            run_text(bytes(t), t, 0xCD, true);
        }
    }
    // Enough names to take each shape: count echoes and particles from the stand-ins.
    for (int i = 0; i < 60; i++) {
        const std::string name = "E" + std::to_string(i);
        const std::string t = particle("RotTransCoordSys MotionState", "Emitter \"" + name + "\" \"\"\nUpdateAtDraw");
        run_text(bytes(t), t, 0xCD, false);
        const std::string u = particle("RotTransCoordSys", "Emitter \"" + name + "\" \"Space\"");
        run_text(bytes(u), u, 0xAA, false);
    }
    // Several, moved, attached and drawn on, the way real files hold them.
    const std::string two = std::string(kHead) + "ParticleSystemActor[2]\n{\n ParticleSystemActor \"First\" RotTransCoordSys MotionState\n {\n BoundingSphere 0.0 0.0 0.0 300.0\n DrawingDisable\n Emitter \"E1\" \"\"\n }\n"
                            " ParticleSystemActor \"Second\" RotTransCoordSys MotionState\n {\n AttachBase \"First\" \"\"\n Emitter \"E2\" \"\"\n PauseParticles\n }\n}\n";
    run_text(bytes(two), two, 0xCD, false);
    // A scene of no particle system's, then one: nothing is left behind by the first.
    const std::string empty = std::string(kHead) + "ParticleSystemActor[0]\n{\n}\n";
    run_text(bytes(empty), empty, 0xCD, false);
}

typedef void(__fastcall* FnStart)(void*, void*, void*, void*, const void*);

// The layers alone, over what is there.
static void run_particle_layers() {
    std::set<std::string> names = {"One", "Two"};
    std::set<std::string> spaces = {"Space"};
    Env env(names, spaces);
    g_fill = 0xCD;
    AtSceneParticleLayer* l[2] = {(AtSceneParticleLayer*)std::malloc(sizeof(AtSceneParticleLayer)), (AtSceneParticleLayer*)std::malloc(sizeof(AtSceneParticleLayer))};
    const auto compare = [&](const char* what) {
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, g_emitters[0], (size_t)g_emitter_count * kEmitterRoom, 41);
        add_range(1, g_emitters[1], (size_t)g_emitter_count * kEmitterRoom, 41);
        same_layer(l[0], l[1]);
        eq("layer's first word's side", 0, 0);
        compare_side_effects(what);
    };
    std::memset(l[0], 0xCD, sizeof(AtSceneParticleLayer));
    std::memset(l[1], 0xCD, sizeof(AtSceneParticleLayer));
    { On on(0); at<Fn0>(0x0005AC90)(l[0], nullptr); }
    { On on(1); new (l[1]) AtSceneParticleLayer; }
    compare("layer made");

    // Nothing to let go of, each word showing what clearing does.
    for (int side = 0; side < 2; side++) {
        l[side]->m_unknown0C = 3; l[side]->m_unknown10 = 4; l[side]->m_count = 5; l[side]->m_unknown18 = 6;
        l[side]->m_color.x = 1.5f; l[side]->m_color.y = 2.5f; l[side]->m_color.z = 3.5f; l[side]->m_color.w = 4.5f;
        l[side]->m_unknown2C = 7; l[side]->m_unknown30 = 8; l[side]->m_unknown34 = 9;
    }
    { On on(0); at<Fn0>(0x0005ACC0)(l[0], nullptr); }
    { On on(1); l[1]->Clear(); }
    compare("layer cleared, with nothing");

    const AtQuaternion color = {1.5f, -2.5f, 3.25f, 0.125f};
    AtSceneParticleEmitter* e[2][2] = {{g_emitters[0], (AtSceneParticleEmitter*)((char*)g_emitters[0] + kEmitterRoom)}, {g_emitters[1], (AtSceneParticleEmitter*)((char*)g_emitters[1] + kEmitterRoom)}};
    // With a colour, with none, over one another and from a stale colour.
    const AtQuaternion* const colors[] = {&color, nullptr, &color, &color, nullptr};
    int n = 0;
    for (const AtQuaternion* c : colors) {
        const int which = n++ & 1;
        { On on(0); at<FnStart>(0x0005B2E0)(l[0], nullptr, g_own[0], e[0][which], c); }
        { On on(1); l[1]->Start(g_own[1], e[1][which], c); }
        compare("layer started");
    }
    // A used layer's words, which only the update would set, and clearing them.
    for (int side = 0; side < 2; side++) {
        l[side]->m_unknown18 = 6; l[side]->m_unknown2C = 7; l[side]->m_unknown30 = 8; l[side]->m_unknown34 = 9;
        l[side]->m_particles[0].m_words[0] = 1;
    }
    compare("layer used");
    { On on(0); at<Fn0>(0x0005ACC0)(l[0], nullptr); }
    { On on(1); l[1]->Clear(); }
    compare("layer cleared");

    // A layer started again drops the used words only by way of Clear, so these go if it asks the wrong field whether it has anything.
    { On on(0); at<FnStart>(0x0005B2E0)(l[0], nullptr, g_own[0], e[0][0], &color); }
    { On on(1); l[1]->Start(g_own[1], e[1][0], &color); }
    for (int side = 0; side < 2; side++) { l[side]->m_unknown2C = 7; l[side]->m_unknown30 = 8; l[side]->m_unknown34 = 9; l[side]->m_unknown18 = 6; }
    { On on(0); at<FnStart>(0x0005B2E0)(l[0], nullptr, g_own[0], e[0][1], nullptr); }
    { On on(1); l[1]->Start(g_own[1], e[1][1], nullptr); }
    compare("layer started over a used one");

    // Emitter null with the rest set: Start asks only the emitter whether to clear.
    { On on(0); at<Fn0>(0x0005ACC0)(l[0], nullptr); }
    { On on(1); l[1]->Clear(); }
    compare("layer cleared again");
    const auto stale = [&] {
        for (int side = 0; side < 2; side++) {
            l[side]->m_unknown18 = 11; l[side]->m_unknown2C = 12; l[side]->m_unknown30 = 13; l[side]->m_unknown34 = 14;
            l[side]->m_color.x = 5.5f; l[side]->m_color.y = 6.5f; l[side]->m_color.z = 7.5f; l[side]->m_color.w = 8.5f;
        }
    };
    stale();
    { On on(0); at<FnStart>(0x0005B2E0)(l[0], nullptr, g_own[0], e[0][0], &color); }
    { On on(1); l[1]->Start(g_own[1], e[1][0], &color); }
    compare("layer with only stale words");
    { On on(0); at<Fn0>(0x0005ACC0)(l[0], nullptr); }
    { On on(1); l[1]->Clear(); }
    stale();
    // No colour, over one that clearing did not leave: the only way to see each of the four zeroes.
    { On on(0); at<FnStart>(0x0005B2E0)(l[0], nullptr, g_own[0], e[0][0], nullptr); }
    { On on(1); l[1]->Start(g_own[1], e[1][0], nullptr); }
    compare("layer with a stale colour and none given");

    // The destructor, which is Clear.
    { On on(0); at<Fn0>(0x0005B2D0)(l[0], nullptr); }
    { On on(1); l[1]->~AtSceneParticleLayer(); }
    compare("layer destroyed");

    // The identity, by itself, and one particle.
    unsigned char q[2][sizeof(AtQuaternion)];
    std::memset(q, 0xCD, sizeof q);
    at<Fn0>(0x00035820)(q[0], nullptr);
    ((AtQuaternion*)q[1])->SetIdentity();
    eq("identity", std::memcmp(q[0], q[1], sizeof q[0]), 0);
    unsigned char p[2][sizeof(AtSceneParticle)];
    std::memset(p, 0xCD, sizeof p);
    at<Fn0>(0x0005FF30)(p[0], nullptr);
    new (p[1]) AtSceneParticle;
    eq("particle", std::memcmp(p[0], p[1], sizeof p[0]), 0);

    // The setters.
    at<void(__cdecl*)(void*)>(0x0004C180)(&g_heap_layer);
    AtSceneParticleSystemActor::SetHeap(&g_heap_layer);
    eq("system's heap", *(void**)(uintptr_t)0x002C7E10 == &g_heap_layer, AtSceneParticleSystemActor::s_heap == &g_heap_layer);
    at<void(__cdecl*)(void*)>(0x0005AAC0)(&g_heap_particle);
    AtSceneParticleLayer::SetHeap(&g_heap_particle);
    eq("layer's heap", *(void**)(uintptr_t)0x002C9C10 == &g_heap_particle, AtSceneParticleLayer::s_heap == &g_heap_particle);
    set_particle_heaps(&g_heap_particle, &g_heap_layer);
    g_hash[0] = g_hash[1] = 0;
}

// The actor alone: the constructor over heap fill, its reset over words set, and the emitter's lookup.
static void run_particle_direct() {
    std::set<std::string> names = {"One", "Two"};
    std::set<std::string> spaces = {"Space"};
    Env env(names, spaces);
    g_fill = 0xCD;
    const auto fresh = [&](int side) {
        char* p = (char*)std::malloc(sizeof(AtSceneParticleSystemActor));
        std::memset(p, 0xCD, sizeof(AtSceneParticleSystemActor));
        if (side == 0) at<Fn0>(0x0004D1D0)(p, nullptr);
        else new (p) AtSceneParticleSystemActor;
        return p;
    };
    char* a[2] = {fresh(0), fresh(1)};
    const auto compare = [&](const char* what) {
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, g_emitters[0], (size_t)g_emitter_count * kEmitterRoom, 41);
        add_range(1, g_emitters[1], (size_t)g_emitter_count * kEmitterRoom, 41);
        same_node(a[0], a[1], sizeof(AtSceneParticleSystemActor));
        same_particle_system((AtSceneParticleSystemActor*)a[0], (AtSceneParticleSystemActor*)a[1]);
        compare_side_effects(what);
    };
    compare("made");
    eq("+F4 left as it was", dword(a[0], 0xF4), 0xCDCDCDCD);

    // Each word showing, then clearing with no layers. Clearing the base needs a real owner.
    const auto stain = [&] {
        for (int side = 0; side < 2; side++) {
            AtSceneParticleSystemActor& x = *(AtSceneParticleSystemActor*)a[side];
            x.m_owner = g_own[side];
            unsigned v = 0x100;
            for (unsigned off = 0x98; off < sizeof(AtSceneParticleSystemActor); off += 4) {
                if (off != 0xAC) *(unsigned*)(a[side] + off) = v++;
            }
            x.m_layers = nullptr;
        }
    };
    stain();
    { On on(0); at<Fn0>(0x0004D310)(a[0], nullptr); }
    { On on(1); ((AtSceneParticleSystemActor*)a[1])->Clear(); }
    compare("cleared with nothing");
    stain();
    { On on(0); at<Fn0>(0x0004C670)(a[0], nullptr); }
    { On on(1); ((AtSceneParticleSystemActor*)a[1])->Reset(); }
    compare("reset");

    // Starting over its own layers leaves the old array behind on both, and the cursor moves on.
    for (int side = 0; side < 2; side++) {
        AtSceneParticleSystemActor& x = *(AtSceneParticleSystemActor*)a[side];
        x.m_owner = g_own[side];
        x.m_emitter = g_emitters[side];
        x.m_unknownA4 = 5;
        x.m_unknownB0 = 2.5f;
    }
    typedef void(__fastcall* FnStartLayers)(void*, void*, void*);
    *(uint32_t*)(uintptr_t)0x002C60C0 = 0x7FE;
    g_atRandomCursor = 0x7FE;
    for (int twice = 0; twice < 3; twice++) {
        { On on(0); at<FnStartLayers>(0x0004C3B0)(a[0], nullptr, g_own[0]); }
        { On on(1); ((AtSceneParticleSystemActor*)a[1])->StartLayers(g_own[1]); }
        compare("started");
        eq("cursor wrapped", *(uint32_t*)(uintptr_t)0x002C60C0, g_atRandomCursor);
    }
    *(uint32_t*)(uintptr_t)0x002C60C0 = 0;
    g_atRandomCursor = 0;

    // Clearing with layers, which a scene never does: it destroys them.
    { On on(0); at<Fn0>(0x0004D310)(a[0], nullptr); }
    { On on(1); ((AtSceneParticleSystemActor*)a[1])->Clear(); }
    compare("cleared with layers");

    // Reading by itself. A scene only reads what it has just made, so this is the only way to reach it over a loaded one, and over stale words on one that is not.
    typedef void(__fastcall* FnRead)(void*, void*, void*, void*, void*);
    const auto read_both = [&](const char* what) {
        Reader x("{\n Emitter \"One\" \"\"\n}\n");
        Reader y("{\n Emitter \"One\" \"\"\n}\n");
        { On on(0); at<FnRead>(0x0004D1F0)(a[0], nullptr, g_own[0], nullptr, &x.tokenizer); }
        { On on(1); ((AtSceneParticleSystemActor*)a[1])->Read(g_own[1], nullptr, &y.tokenizer); }
        compare(what);
    };
    for (int side = 0; side < 2; side++) ((AtSceneParticleSystemActor*)a[side])->m_unknownB4 = 3.5f;
    read_both("read, not loaded, with a stale word");
    for (int side = 0; side < 2; side++) ((AtSceneParticleSystemActor*)a[side])->m_unknownB4 = 4.5f;
    read_both("read over a loaded one");

    // The complete destructor, with its layers, on a copy of nothing else.
    { On on(0); at<Fn0>(0x0004D390)(a[0], nullptr); }
    { On on(1); ((AtSceneParticleSystemActor*)a[1])->~AtSceneParticleSystemActor(); }
    g_recording = false;
    for (int side = 0; side < 2; side++) eq("destroyed: no layers", raw(a[side], 0xAC) == nullptr, true);
    compare_side_effects("destroyed");

    // The deleter, with and without freeing, one and an array.
    {
        char* d[2] = {fresh(0), fresh(1)};
        { On on(0); eq("deleter returns", (uintptr_t)at<FnDel>(0x0003B920)(d[0], nullptr, 0), (uintptr_t)d[0]); }
        { On on(1); ((AtSceneParticleSystemActor*)d[1])->~AtSceneParticleSystemActor(); }
        compare_side_effects("deleter, not freeing");
    }

    // The lookup, directly: in each table and neither, with null and empty name spaces.
    typedef void*(__fastcall* FnFind)(void*, void*, const char*, const char*);
    static const char* const asks[][2] = {{"One", nullptr}, {"One", ""}, {"One", "Space"}, {"Two", "Space"}, {"Gone", nullptr}, {"Gone", "Space"}, {"One", "Nothing"}};
    for (const auto& ask : asks) {
        void* x;
        void* y;
        { On on(0); x = at<FnFind>(0x0004C370)(g_own[0], nullptr, ask[0], ask[1]); }
        { On on(1); y = g_own[1]->FindEmitter(ask[0], ask[1]); }
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, g_emitters[0], (size_t)g_emitter_count * kEmitterRoom, 41);
        add_range(1, g_emitters[1], (size_t)g_emitter_count * kEmitterRoom, 41);
        eq("emitter found", canon(0, x), canon(1, y));
        compare_side_effects("emitter looked for");
    }
    g_hash[0] = g_hash[1] = 0;
}

static void run_particle_shortages() {
    const std::string busy = particle("RotTransCoordSys", "Emitter \"Self\" \"\"");
    set_particle_heaps(nullptr, &g_heap_layer);
    run_text(bytes(busy), "no heap for the layers", 0xCD, true);
    set_particle_heaps(&g_heap_particle, nullptr);
    run_text(bytes(busy), "no heap for particles", 0xCD, true);
    set_particle_heaps(&g_heap_particle, &g_heap_layer);
    g_refuse_heap = &g_heap_particle;
    run_text(bytes(busy), "no memory for the layers", 0xCD, true);
    g_refuse_heap = &g_heap_layer;
    run_text(bytes(busy), "no memory for particles", 0xCD, true);
    g_refuse_heap = nullptr;
    g_fail_alloc = false;
    g_refusing = 0;
}

static void run_particle_tests() {
    run_particle_table(); std::printf("run_particle_table done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_particle_layers(); std::printf("run_particle_layers done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_particle_direct(); std::printf("run_particle_direct done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_particle_words(); std::printf("run_particle_words done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_particle_shortages(); std::printf("run_particle_shortages done: %llu comparisons, %d differences\n", g_checks, g_failures);
}

int main(int argc, char** argv) { return oracle_main(argc, argv, run_particle_tests); }
