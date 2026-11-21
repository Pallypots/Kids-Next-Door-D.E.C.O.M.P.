// Differential oracle for the group, water, lens flare and sound classes of a scene node and the walks that put a scene into a space graph (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-19-scenenodes.md.
//
//   oracle_scenenodes <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Built on oracle_asn.cpp, whose harness it shares. No real .ASN file names any of these classes: every case here is written text, or a call made directly. The real files are still loaded at the end, as in the scene suite, and with
// these classes real on both sides they show nothing changed for the classes they do have. What stands in for what: the water drawers, the lens flares, the sound owner's emitters and segments and the two emitter slots a sound node calls
// are the harness's, the same on both sides. The space graphs are real on both sides, each read from the same text by its own reader.
#define ORACLE_ACTOR
#include "oracle_asn.cpp"

#include "at/AtHeap.h"
#include "at/AtSceneDatabaseLoader.h"

// Three models to put in groups, before the classes that name them; the third has no coordinate system. Groups name only these and PlainOne, a stand-in model: a jointed model's slots 32 and 33 are not recovered.
static const char kModels[] = "ModelActor[4]\n{\n ModelActor \"MA\" RotTransCoordSys\n {\n }\n ModelActor \"MB\" RotTransCoordSys\n {\n DrawingEnable\n }\n ModelActor \"MC\"\n {\n }\n ModelActor \"PM\" RotTransCoordSys\n {\n }\n}\n";

static std::string with_models(const std::string& cls, const std::string& header, const std::string& body) {
    return std::string(kHead) + kModels + cls + "[1]\n{\n " + cls + " \"Self\" " + header + "\n {\n" + body + "\n }\n}\n";
}

static void run_cases(const std::string& cls, const char* const* bodies, size_t count, const std::vector<const char*>& headers, bool models) {
    for (size_t i = 0; i < count; i++) {
        for (const char* h : headers) {
            const std::string t = models ? with_models(cls, h, bodies[i]) : one(cls, h, bodies[i]);
            run_text(bytes(t), t, 0xCD, true);
        }
    }
}

// ---- the groups -----------------------------------------------------------------------------------------------------------------

static void run_group_words() {
    static const char* const lists[] = {
        "Actor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}", "Actor [3]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n \"MC\" \"\"\n}", "Actor [1]\n{\n \"PlainOne\" \"\"\n}", "Actor [1]\n{\n \"PlainOne\" \"Space\"\n}", "Actor [1]\n{\n \"PM\" \"Space\"\n}", "Actor [2]\n{\n \"PM\" \"\"\n \"PM\" \"Space\"\n}",
        "Actor [2]\n{\n \"MB\" \"\"\n \"MA\" \"\"\n}", "Actor [2]\n{\n \"MA\" \"\"\n}", "Actor [0]\n{\n}", "Actor [2]\nDrawingEnable", "Actor [2]\nActor [1]\n{\n \"MA\" \"\"\n}",
        "Actor [1]\n{\n \"MA\" \"\"\n}\nActor [1]\n{\n \"MB\" \"\"\n}", "Actor [1]\n{\n \"MA\" \"\"\n}\nActor [2]\n{\n \"MB\" \"\"\n \"MC\" \"\"\n}", "Actor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}\nActor [0]\n{\n}",
        "Actor [1]\n{\n \"XGone\" \"\"\n}", "Actor [1]\n{\n \"MA\" \"XNone\"\n}", "Actor [2]\n{\n \"MA\" \"\"\n \"MA\" \"\"\n}", "Actor [1]\n{\n \"MA\" \"\"\n", "Actor [x]\n{\n}", "Actor 2\n{\n}", "Actor [1]\n{\n 5 5\n}",
        "DrawingEnable\nActor [1]\n{\n \"MA\" \"\"\n}", "Other", "5", "Position 1.5 2.5 3.5\nActor [1]\n{\n \"MA\" \"\"\n}", "Actor [1]\n{\n \"MA\" \"\"\n}\nPosition 1.5 2.5 3.5",
    };
    const size_t n = sizeof lists / sizeof lists[0];
    run_cases("GroupActor", lists, n, {"RotTransCoordSys", "", "RotTransCoordSys MotionState"}, true);
    run_cases("SwitchGroupActor", lists, n, {"RotTransCoordSys"}, true);

    static const char* const switches[] = {
        "Switch 2", "Switch -1", "Switch 0\nSwitch 7", "Switch x", "Switch", "Switch 1\nActor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}", "Actor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}\nSwitch 1", "Lod [1]\n{\n \"MA\" \"\" 1.0\n}",
    };
    run_cases("SwitchGroupActor", switches, sizeof switches / sizeof switches[0], {"RotTransCoordSys", ""}, true);

    static const char* const lods[] = {
        "Lod [3]\n{\n \"MA\" \"\" 5.0\n \"MB\" \"\" 2.5\n \"MC\" \"\" 7.25\n}", "Lod [3]\n{\n \"MA\" \"\" 1.0\n \"MB\" \"\" 2.0\n \"MC\" \"\" 3.0\n}", "Lod [3]\n{\n \"MA\" \"\" 3.0\n \"MB\" \"\" 2.0\n \"MC\" \"\" 1.0\n}",
        "Lod [3]\n{\n \"MA\" \"\" 2.0\n \"MB\" \"\" 2.0\n \"MC\" \"\" 1.0\n}", "Lod [2]\n{\n \"MA\" \"\" -4.5\n \"MB\" \"\" 3.0\n}", "Lod [2]\n{\n \"MA\" \"\" 1e30\n \"MB\" \"\" 1e20\n}", "Lod [2]\n{\n \"MA\" \"\" 0.1\n \"MB\" \"\" 1e-30\n}",
        "Lod [1]\n{\n \"PlainOne\" \"\" 2.0\n}", "Lod [1]\n{\n \"PlainOne\" \"Space\" 2.0\n}", "Lod [1]\n{\n \"PM\" \"Space\" 2.0\n}", "Lod [2]\n{\n \"PM\" \"\" 3.0\n \"PM\" \"Space\" 2.0\n}", "Lod [2]\n{\n \"MA\" \"\" 4\n \"MB\" \"\" 3\n}",
        "Lod [2]\n{\n \"MA\" \"\" 4.0\n}", "Lod [0]\n{\n}", "Lod [2]\nDrawingEnable", "Lod [2]\nLod [2]\n{\n \"MA\" \"\" 1.0\n \"MB\" \"\" 0.5\n}",
        // A second list goes in after the first, past the room: the original checks only that list's own count.
        "Lod [2]\n{\n \"MA\" \"\" 4.0\n \"MB\" \"\" 1.0\n}\nLod [1]\n{\n \"MC\" \"\" 2.0\n}", "Lod [1]\n{\n \"MA\" \"\" 4.0\n}\nLod [2]\n{\n \"MB\" \"\" 1.0\n \"MC\" \"\" 9.0\n}",
        "Lod [1]\n{\n \"XGone\" \"\" 1.0\n}", "Lod [1]\n{\n \"MA\" \"XNone\" 1.0\n}", "Lod [2]\n{\n \"MA\" \"\" 1.0\n \"MA\" \"\" 2.0\n}", "Lod [1]\n{\n \"MA\" \"\" x\n}", "Lod [1]\n{\n \"MA\" \"\" 1.0\n", "Lod [x]", "Lod 1",
        "Actor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}", "Lod [1]\n{\n \"MA\" \"\" 1.0\n}\nActor [1]\n{\n \"MB\" \"\"\n}", "Other", "DrawingEnable\nLod [1]\n{\n \"MB\" \"\" 1.5\n}",
    };
    run_cases("LodGroupActor", lods, sizeof lods / sizeof lods[0], {"RotTransCoordSys", "", "RotTransCoordSys MotionState"}, true);

    // Groups of groups, sharing chains, and two groups after one child.
    for (const std::string& t : {
             std::string(kHead) + kModels + "GroupActor[2]\n{\n GroupActor \"GA\" RotTransCoordSys\n {\n Actor [1]\n {\n \"MA\" \"\"\n }\n }\n GroupActor \"GB\" RotTransCoordSys\n {\n Actor [2]\n {\n \"GA\" \"\"\n \"MB\" \"\"\n }\n }\n}\n",
             std::string(kHead) + kModels + "GroupActor[1]\n{\n GroupActor \"GA\" RotTransCoordSys\n {\n Actor [1]\n {\n \"MA\" \"\"\n }\n }\n}\nLodGroupActor[1]\n{\n LodGroupActor \"LA\" RotTransCoordSys\n {\n Lod [2]\n {\n \"MA\" \"\" 1.0\n \"MB\" \"\" 2.0\n }\n }\n}\n",
             std::string(kHead) + kModels + "LodGroupActor[2]\n{\n LodGroupActor \"LA\" RotTransCoordSys\n {\n Lod [1]\n {\n \"MA\" \"\" 1.0\n }\n }\n LodGroupActor \"LB\"\n {\n Lod [2]\n {\n \"LA\" \"\" 3.0\n \"MB\" \"\" 2.0\n }\n }\n}\n",
             std::string(kHead) + kModels + "SwitchGroupActor[2]\n{\n SwitchGroupActor \"SA\" RotTransCoordSys\n {\n Switch 1\n Actor [2]\n {\n \"MA\" \"\"\n \"MB\" \"\"\n }\n }\n SwitchGroupActor \"SB\" RotTransCoordSys\n {\n Actor [1]\n {\n \"SA\" \"\"\n }\n }\n}\n",
         }) {
        run_text(bytes(t), t, 0xCD, true);
        run_text(bytes(t), t, 0x5A, true);
    }
}

// ---- water, lens flares and sound ---------------------------------------------------------------------------------------------------

static std::string mask(int from) {
    std::string m = "Mask\n {\n";
    for (int i = 0; i < 32; i++) m += " " + std::to_string(from + i * 3);
    return m + "\n }\n";
}

static void run_water_words() {
    const std::string full = "WaterPatch [1]\n{\n WaterPatch\n {\n PositionRect 1.5 2.5 3.5 4.5\n TxtCoordRect 0.25 0.5 0.75 1.0\n Neighbors 1 2 3 -1\n " + mask(-7) + " }\n}";
    const std::string two = "WaterPatch [2]\n{\n WaterPatch\n {\n Neighbors 4 5 6 7\n }\n WaterPatch\n {\n " + mask(100) + " PositionRect -1.5 0.0 2.0 1e10\n }\n}";
    // Every water actor here has patches: a second one without would be handed the first one's freed patches, and free them again, which the harness cannot survive.
    const std::vector<std::string> bodies = {
        full, two, "WaterPatch [1]\n{\n WaterPatch\n {\n }\n}", "WaterPatch [0]\n{\n}", "WaterPatch [1]\n{\n WaterPatch\n {\n TxtCoordRect 9.5 8.5 7.5 6.5\n TxtCoordRect 1.0 2.0 3.0 4.0\n }\n}",
        "WaterPatch [1]\n{\n WaterPatch\n {\n Other\n Neighbors 1 1 1 1\n }\n}", "WaterPatch [1]\n{\n WaterPatch\n {\n 5\n }\n}", "WaterPatch [1]\n{\n Other\n {\n }\n}", "WaterPatch [1]\n{\n WaterPatch\n PositionRect 1 2 3 4\n}",
        "WaterPatch [1]\n{\n WaterPatch\n {\n Neighbors 1 2 x 4\n }\n}", "WaterPatch [1]\n{\n WaterPatch\n {\n Mask\n {\n 1 2 3\n }\n }\n}", "WaterPatch [1]\n{\n WaterPatch\n {\n }\n WaterPatch\n {\n }\n}", "WaterPatch [2]\n{\n WaterPatch\n {\n }\n}",
        "WaterPatch 1\n{\n}", "WaterPatch [x]\n{\n}",
        full + "\nMatAssignmentOffset 7", full + "\nMatAssignmentOffset -3\nMatAssignmentOffset 9", full + "\nPlaybackRate 30.5", full + "\nPlaybackRate 4", full + "\nDampingFactor 0.95", full + "\nGenerationFactor 0.125",
        full + "\nDampingFactor x", full + "\nMatAssignmentOffset 1.5", "PlaybackRate 12.25\nDampingFactor 0.5\nGenerationFactor 2.5\nMatAssignmentOffset 2\n" + full,
        // Twice: the first patches are left behind, and the count is the second's.
        full + "\n" + two, two + "\n" + full,
        full + "\nDrawingEnable", full + "\nMatAssignment \"Self\" \"\"", full + "\nOther", full + "\n5", full + "\nCollisionEnable\nLineSegIntersectEnable",
    };
    for (const std::string& b : bodies) {
        for (const char* h : {"RotTransCoordSys", "", "RotTransCoordSys MotionState"}) {
            const std::string t = one("WaterActor", h, b);
            run_text(bytes(t), t, 0xCD, true);
        }
    }
    // With none at all: the drawer gets no patches, and the null is freed.
    for (const char* b : {"", "PlaybackRate 8.0"}) {
        const std::string t = one("WaterActor", "RotTransCoordSys", b);
        run_text(bytes(t), t, 0xCD, true);
    }
    const std::string pair = std::string(kHead) + "WaterActor[2]\n{\n WaterActor \"W1\" RotTransCoordSys\n {\n " + full + "\n DampingFactor 0.25\n }\n WaterActor \"W2\"\n {\n " + two + "\n }\n}\n";
    run_text(bytes(pair), pair, 0xCD, false);
    run_text(bytes(pair), pair, 0x5A, false);
}

static void run_flare_words() {
    static const char* const bodies[] = {
        "LensFlare \"Fl\" \"\"", "LensFlare \"Fl\" \"Space\"", "LensFlare \"XGone\" \"\"", "LensFlare \"Fl\" \"XNone\"", "LensFlare \"Fl\" \"\"\nLensFlare \"Gl\" \"\"", "LensFlare", "LensFlare 5 5",
        "LineOfSightTestEnable", "LineOfSightTestDisable", "LineOfSightTestEnable\nLineOfSightTestDisable", "LineOfSightTestDisable\nLineOfSightTestEnable",
        "BoundingSphere 1.5 2.5 3.5 4.5", "BoundingSphere 1.5 2.5 3.5 4.5\nLensFlare \"Fl\" \"\"", "DrawingEnable\nLensFlare \"Fl\" \"\"", "Other", "5", "Position 1.5 2.5 3.5", "CollisionEnable\nLineOfSightTestEnable",
        "", "MatAssignment \"Self\" \"\"",
    };
    run_cases("LensFlareActor", bodies, sizeof bodies / sizeof bodies[0], {"RotTransCoordSys", "", "RotTransCoordSys ScaleCoordSys"}, false);
}

static void run_sound_words() {
    static const char* const bodies[] = {
        "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" 3", "Emitter \"Qem\" \"\"\nPlaySegment \"Seg\" \"\" 3", "Emitter \"Em\" \"Space\"\nPlaySegment \"Seg\" \"Space\" -1", "PlaySegment \"Seg\" \"\" 1\nEmitter \"Em\" \"\"",
        "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" 1\nPlaySegment \"Teg\" \"\" 2\nPlaySegment \"Seg\" \"Space\" 3", "Emitter \"Em\" \"\"", "PlaySegment \"Seg\" \"\" 4", "Emitter \"XGone\" \"\"\nPlaySegment \"Seg\" \"\" 1",
        "Emitter \"Em\" \"XNone\"\nPlaySegment \"Seg\" \"\" 1", "Emitter \"Em\" \"\"\nPlaySegment \"XGone\" \"\" 1", "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"XNone\" 1", "Emitter \"Em\" \"\"\nEmitter \"Qem\" \"\"\nPlaySegment \"Seg\" \"\" 1",
        "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" x", "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\"", "Emitter", "PlaySegment 5", "Emitter \"Em\" \"\"\nPlaySegment \"Mute\" \"\" 1", "PlaySegment \"Mute\" \"\" 1\nEmitter \"Em\" \"\"",
        "Emitter \"Em\" \"\"\nPosition 1.5 2.5 3.5\nPlaySegment \"Seg\" \"\" 1", "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" 1\nCollisionEnable\nAttachBase \"Target\" \"\"", "Other", "5", "", "DrawingEnable",
        "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" 1\nMeshless",
    };
    run_cases("SoundNode", bodies, sizeof bodies / sizeof bodies[0], {"RotTransCoordSys", "", "RotTransCoordSys MotionState"}, false);
    // Two sound nodes: the second's list is its own, since the first's Read took everything.
    const std::string pair = std::string(kHead) + "SoundNode[2]\n{\n SoundNode \"S1\" RotTransCoordSys\n {\n Emitter \"Em\" \"\"\n PlaySegment \"Seg\" \"\" 1\n }\n SoundNode \"S2\"\n {\n Emitter \"Qem\" \"Space\"\n PlaySegment \"Teg\" \"\" 2\n PlaySegment \"Seg\" \"\" 3\n }\n}\n";
    run_text(bytes(pair), pair, 0xCD, false);
}

// ---- driving the classes directly -----------------------------------------------------------------------------------------------------

typedef void(__fastcall* FnInsert)(void*, void*, void*, float);
typedef void(__fastcall* FnBody)(void*, void*, void*, void*, int);
typedef void(__fastcall* FnMask)(void*, void*, unsigned);
typedef void(__fastcall* FnFrom)(void*, void*, void*, const void*);
typedef void(__fastcall* FnWorld)(void*, void*, const void*, const void*, const void*);

static const int kMyKinds[] = {kAtActorWater, kAtActorLensFlare, kAtActorGroup, kAtActorSwitchGroup, kAtActorLodGroup, kAtActorSound};
static const uint32_t kOriginalClear[kAtActorKinds] = {0, 0, 0, 0x0004E570, 0x0004ECA0, 0x0004D7E0, 0x0004D7E0, 0x0004DE30, 0x0004F0B0};

static char* fresh(int side, int kind, unsigned char fill) {
    char* p = (char*)std::malloc(0x200);
    std::memset(p, fill, 0x200);
    On on(side);
    if (side == 0) at<Fn0>(kOriginalCtor[kind])(p, nullptr);
    else AtSceneActorsConstruct(kind, p, 1);
    return p;
}

static void compare_pair(const char* what, char* const* a, int kind) {
    g_recording = false;
    g_ranges[0].clear();
    g_ranges[1].clear();
    for (int side = 0; side < 2; side++) {
        add_range(side, g_fake_plain[side], kSizes[kAtActorModel], 2);
        add_range(side, g_fake_jointed[side], kSizes[kAtActorJointedModel], 1);
        add_range(side, g_sound_emitters[side], (size_t)g_sound_emitter_count * kSoundEmitterRoom, 42);
    }
    same_node(a[0], a[1], kSizes[kind]);
    compare_side_effects(what);
}

static void run_direct() {
    std::set<std::string> names = {"One", "Two", "PlainOne", "Em", "Qem"};
    std::set<std::string> spaces = {"Space"};
    Env env(names, spaces);
    char* second_child[2] = {make_actor(0, kAtActorModel), make_actor(1, kAtActorModel)};
    for (int side = 0; side < 2; side++) {
        ((AtSceneNode*)second_child[side])->m_owner = g_own[side];
        ((AtSceneNode*)second_child[side])->m_flags = 0;
    }
    for (const int kind : kMyKinds) {
        for (const unsigned char fill : {0xCD, 0x00}) {
            g_fill = fill;
            char* a[2] = {fresh(0, kind, fill), fresh(1, kind, fill)};
            compare_pair("made", a, kind);
            // Each word stained, then cleared directly: what clearing leaves and what it lets go of.
            for (int side = 0; side < 2; side++) {
                AtSceneNode& n = *(AtSceneNode*)a[side];
                n.m_owner = g_own[side];
                n.m_flags = AtSceneNode::kLoaded | AtSceneNode::kCollision | AtSceneNode::kLineSegIntersect | 0x400 | 0x800 | 0x2000 | 0x4000 | 0x10000;
                n.m_sphere.radius = 3.5f;
                n.m_worldSphere.radius = 4.5f;
                if (kind == kAtActorSound) { raw(a[side], 0x6C) = g_sound_emitters[side]; continue; }
                for (unsigned off = 0x98; off < kSizes[kind]; off += 4) {
                    if (kind == kAtActorLensFlare && off >= 0xA0 && off < 0x138) continue;
                    *(uint32_t*)(a[side] + off) = 0x100 + off;
                }
                switch (kind) {
                case kAtActorWater: raw(a[side], 0x98) = new (std::malloc(sizeof(TestWaterDrawer))) TestWaterDrawer(side, 77); break;
                case kAtActorGroup: case kAtActorSwitchGroup: case kAtActorLodGroup: {
                    AtSceneGroupActor& g = *(AtSceneGroupActor*)a[side];
                    g.m_count = 2;
                    g.m_children = (AtSceneActor**)std::malloc(8);
                    g.m_children[0] = (AtSceneActor*)g_fake_plain[side];
                    g.m_children[1] = (AtSceneActor*)second_child[side];
                    ((AtSceneActor*)g_fake_plain[side])->m_sharedFrom = (AtSceneActor*)a[side];
                    ((AtSceneActor*)second_child[side])->m_sharedFrom = (AtSceneActor*)a[side];
                    if (kind == kAtActorLodGroup) {
                        float* d = (float*)std::malloc(8);
                        d[0] = 1.5f;
                        d[1] = 2.5f;
                        ((AtSceneLodGroupActor*)a[side])->m_distances = d;
                    }
                    break;
                }
                case kAtActorLensFlare: {
                    // Words of the query that its Clear resets, and storage not its own, which it does not free.
                    AtSegmentQuery& q = ((AtSceneLensFlareActor*)a[side])->m_query;
                    q.m_flags = AtSegmentHitList::kNotOwned;
                    q.m_capacity = 7;
                    q.m_count = 5;
                    q.m_unknown8C = 9;
                    q.m_unknown60[0] = 11;
                    break;
                }
                default: break;
                }
            }
            compare_pair("stained", a, kind);
            // The water's class too, which only its Clear puts back.
            if (kind == kAtActorWater) for (int side = 0; side < 2; side++) ((AtSceneNode*)a[side])->m_class = 77;
            { On on(0); at<Fn0>(kOriginalClear[kind])(a[0], nullptr); }
            { On on(1); ((AtSceneNode*)a[1])->Clear(); }
            compare_pair("cleared", a, kind);
            same_node(second_child[0], second_child[1], kSizes[kAtActorModel]);
            same_node(g_fake_plain[0], g_fake_plain[1], kSizes[kAtActorModel]);
            { On on(0); at<Fn0>(kOriginalClear[kind])(a[0], nullptr); }
            { On on(1); ((AtSceneNode*)a[1])->Clear(); }
            compare_pair("cleared twice", a, kind);
            // The complete destructor, over a cleared one; then the deleter, not freeing and freeing.
            { On on(0); at<Fn0>(kOriginalDtor[kind])(a[0], nullptr); }
            { On on(1); AtSceneActorsDestroy(kind, a[1], 1); }
            // What a destroyed one leaves in its memory: the words each destructor wrote last.
            same_node(a[0], a[1], kSizes[kind]);
            compare_side_effects("destroyed");
            char* d[2] = {fresh(0, kind, fill), fresh(1, kind, fill)};
            { On on(0); eq("deleter returns", (uintptr_t)at<FnDel>(kOriginalDeleter[kind])(d[0], nullptr, 0), (uintptr_t)d[0]); }
            { On on(1); AtSceneActorsDestroy(kind, d[1], 1); }
            compare_side_effects("deleter, not freeing");
            char* f = fresh(0, kind, fill);
            AtSceneNode* made = nullptr;
            switch (kind) {
            case kAtActorWater: made = new AtSceneWaterActor; break;
            case kAtActorLensFlare: made = new AtSceneLensFlareActor; break;
            case kAtActorGroup: made = new AtSceneGroupActor; break;
            case kAtActorSwitchGroup: made = new AtSceneSwitchGroupActor; break;
            case kAtActorLodGroup: made = new AtSceneLodGroupActor; break;
            default: made = new AtSceneSoundNode; break;
            }
            compare_logs("made to delete");
            g_hash[0] = g_hash[1] = 0;
            { On on(0); eq("deleter returns, freeing", (uintptr_t)at<FnDel>(kOriginalDeleter[kind])(f, nullptr, 1), (uintptr_t)f); }
            { On on(1); delete made; }
            compare_side_effects("deleter, freeing");
            ((AtSceneActor*)g_fake_plain[0])->m_sharedFrom = nullptr;
            ((AtSceneActor*)g_fake_plain[1])->m_sharedFrom = nullptr;
            ((AtSceneActor*)g_fake_plain[0])->m_coordSys = g_fake_cs[0];
            ((AtSceneActor*)g_fake_plain[1])->m_coordSys = g_fake_cs[1];
            for (int side = 0; side < 2; side++) ((AtSceneActor*)second_child[side])->m_sharedFrom = nullptr;
        }
    }

    // The complete destructor straight over stained words, with nothing to let go of: what each destructor's own clearing writes.
    for (const int kind : {kAtActorWater, kAtActorLensFlare, kAtActorGroup, kAtActorLodGroup, kAtActorSwitchGroup}) {
        char* a[2] = {fresh(0, kind, 0xCD), fresh(1, kind, 0xCD)};
        for (int side = 0; side < 2; side++) {
            AtSceneNode& n = *(AtSceneNode*)a[side];
            n.m_owner = g_own[side];
            n.m_sphere.radius = 3.5f;
            n.m_worldSphere.radius = 4.5f;
            for (unsigned off = 0x9C; off < kSizes[kind]; off += 4) {
                if (kind == kAtActorLensFlare && off >= 0xA0 && off < 0x138) continue;
                if (kind != kAtActorWater && kind != kAtActorLensFlare && (off == 0xA4 || off == 0xA8)) continue;  // the children and the distances
                *(uint32_t*)(a[side] + off) = 0x100 + off;
            }
            if (kind == kAtActorLensFlare) raw(a[side], 0x98) = (void*)(uintptr_t)0x777;
        }
        { On on(0); at<Fn0>(kOriginalDtor[kind])(a[0], nullptr); }
        { On on(1); AtSceneActorsDestroy(kind, a[1], 1); }
        g_recording = false;
        same_node(a[0], a[1], kSizes[kind]);
        compare_side_effects("destroyed over stained words");
    }

    // A group's first list with no list after the count, over a count left from before: the count is put to zero.
    for (const int kind : {kAtActorGroup, kAtActorSwitchGroup}) {
        char* a[2] = {fresh(0, kind, 0xCD), fresh(1, kind, 0xCD)};
        for (int side = 0; side < 2; side++) ((AtSceneGroupActor*)a[side])->m_count = 5;
        Reader x("[2]\nDrawingEnable\n"), y("[2]\nDrawingEnable\n");
        { On on(0); at<FnBody>(kind == kAtActorGroup ? 0x0004D600 : 0x0004E1D0)(a[0], nullptr, g_own[0], &x.tokenizer, kAtWordActor); }
        { On on(1); ((AtSceneNode*)a[1])->ReadBody(g_own[1], &y.tokenizer, kAtWordActor); }
        compare_pair("no list over a count", a, kind);
    }

    // A lens flare read by itself, not loaded, over a radius of its own: Read sets it before anything else does.
    {
        char* a[2] = {fresh(0, kAtActorLensFlare, 0xCD), fresh(1, kAtActorLensFlare, 0xCD)};
        for (int side = 0; side < 2; side++) {
            ((AtSceneNode*)a[side])->m_sphere.radius = 3.5f;
            ((AtSceneNode*)a[side])->m_worldSphere.radius = 4.5f;
        }
        Reader x("{\n}\n"), y("{\n}\n");
        { On on(0); at<void(__fastcall*)(void*, void*, void*, void*, void*)>(0x0004EBA0)(a[0], nullptr, g_own[0], nullptr, &x.tokenizer); }
        { On on(1); ((AtSceneNode*)a[1])->Read(g_own[1], nullptr, &y.tokenizer); }
        compare_pair("flare read by itself", a, kAtActorLensFlare);
    }

    // Emitter with no sound owner leaves the emitter it had.
    {
        char* a[2] = {fresh(0, kAtActorSound, 0xCD), fresh(1, kAtActorSound, 0xCD)};
        AtNameTableOwner* had[2];
        for (int side = 0; side < 2; side++) {
            raw(a[side], 0x6C) = g_sound_emitters[side];
            had[side] = g_own[side]->m_soundOwner;
            g_own[side]->m_soundOwner = nullptr;
        }
        Reader x("\"Em\" \"\"\n"), y("\"Em\" \"\"\n");
        { On on(0); at<FnBody>(0x0004F0D0)(a[0], nullptr, g_own[0], &x.tokenizer, kAtWordEmitter); }
        { On on(1); ((AtSceneNode*)a[1])->ReadBody(g_own[1], &y.tokenizer, kAtWordEmitter); }
        for (int side = 0; side < 2; side++) g_own[side]->m_soundOwner = had[side];
        compare_pair("emitter with no sound owner", a, kAtActorSound);
    }

    // A level-of-detail group's place for each new child, over orders and distances that do not compare.
    {
        g_fill = 0xCD;
        char* a[2] = {fresh(0, kAtActorLodGroup, 0xCD), fresh(1, kAtActorLodGroup, 0xCD)};
        for (int side = 0; side < 2; side++) {
            AtSceneLodGroupActor& g = *(AtSceneLodGroupActor*)a[side];
            g.m_owner = g_own[side];
            g.m_capacity = 16;
            g.m_children = (AtSceneActor**)std::malloc(16 * 4);
            g.m_distances = (float*)std::malloc(16 * 4);
        }
        float nan;
        const uint32_t nan_bits = 0x7FC00000;
        std::memcpy(&nan, &nan_bits, 4);
        const float order[] = {4.0f, 1.0f, 9.0f, 4.0f, nan, 0.0f, -0.0f, 2.5f, nan, 16.0f, 1.0f};
        for (const float d : order) {
            for (int side = 0; side < 2; side++) ((AtSceneActor*)g_fake_plain[side])->m_sharedFrom = nullptr;
            { On on(0); at<FnInsert>(0x0004DAC0)(a[0], nullptr, g_fake_plain[0], d); }
            { On on(1); ((AtSceneLodGroupActor*)a[1])->Insert((AtSceneActor*)g_fake_plain[1], d); }
            compare_pair("inserted", a, kAtActorLodGroup);
            same_node(g_fake_plain[0], g_fake_plain[1], kSizes[kAtActorModel]);
        }
        for (int side = 0; side < 2; side++) {
            AtSceneLodGroupActor& g = *(AtSceneLodGroupActor*)a[side];
            g.m_count = 0;
            ((AtSceneActor*)g_fake_plain[side])->m_sharedFrom = nullptr;
            ((AtSceneActor*)g_fake_plain[side])->m_coordSys = g_fake_cs[side];
        }
    }

    // A word read by itself, with no heap for it: the report, and then what follows on both sides.
    {
        for (const auto& c : {std::make_pair(kAtActorWater, std::string("[1]\n{\n WaterPatch\n {\n }\n}\n")), std::make_pair(kAtActorSound, std::string("\"Em\" \"\" 4\n"))}) {
            char* a[2] = {fresh(0, c.first, 0xCD), fresh(1, c.first, 0xCD)};
            *(void**)(uintptr_t)0x002C60B0 = nullptr;
            AtIoTokenFile::s_heap = nullptr;
            const int word = c.first == kAtActorWater ? kAtWordWaterPatch : kAtWordPlaySegment;
            Reader x(c.second), y(c.second);
            g_context = "no heap: " + c.second;
            const bool oc = guarded([&] { On on(0); at<FnBody>(c.first == kAtActorWater ? 0x0004E780 : 0x0004F0D0)(a[0], nullptr, g_own[0], &x.tokenizer, word); });
            const bool rc = guarded([&] { On on(1); ((AtSceneNode*)a[1])->ReadBody(g_own[1], &y.tokenizer, word); });
            eq("crash", oc, rc);
            *(void**)(uintptr_t)0x002C60B0 = &g_heap_file;
            AtIoTokenFile::s_heap = &g_heap_file;
            compare_side_effects("no heap");
            eq("patches", *(void**)(uintptr_t)0x002C7E14 != nullptr, AtSceneWaterActor::s_patches != nullptr);
            eq("pending", *(void**)(uintptr_t)0x002C7E18 != nullptr, AtSceneSoundNode::s_pending != nullptr);
            if (c.first == kAtActorSound && AtSceneSoundNode::s_pending && *(void**)(uintptr_t)0x002C7E18) {
                const AtScenePendingSegment* o = *(const AtScenePendingSegment**)(uintptr_t)0x002C7E18;
                const AtScenePendingSegment* r = AtSceneSoundNode::s_pending;
                eq("pending segment", (uintptr_t)o->m_segment, (uintptr_t)r->m_segment);
                eq("pending start", o->m_start, r->m_start);
                eq("pending next", o->m_next != nullptr, r->m_next != nullptr);
            }
            *(void**)(uintptr_t)0x002C7E14 = nullptr;
            AtSceneWaterActor::s_patches = nullptr;
            *(void**)(uintptr_t)0x002C7E18 = nullptr;
            AtSceneSoundNode::s_pending = nullptr;
        }
    }
    g_hash[0] = g_hash[1] = 0;
}

// The coordinate system functions a sound node reaches, on systems with and without a parent and children.
static void run_world() {
    std::set<std::string> names = {"One"};
    std::set<std::string> spaces = {"Space"};
    Env env(names, spaces);
    for (unsigned shape = 0; shape < 8; shape++) {
        AtCoordSys* cs[2];
        AtCoordSys* from[2];
        AtCoordSys* parent[2];
        AtCoordSys* kids[2][2];
        for (int side = 0; side < 2; side++) {
            cs[side] = make_cs(side);
            from[side] = make_cs(side);
            parent[side] = make_cs(side);
            mix((Mat*)cs[side]->m_world, 'a', shape, 1, 2);
            mix((Mat*)from[side]->m_world, 'b', shape, 3, 4);
            mix((Mat*)parent[side]->m_world, 'c', shape, 5, 6);
            cs[side]->m_dirty = 0x10 | shape;
            parent[side]->m_dirty = shape & 1 ? 0x20 : 0;
            for (int k = 0; k < 2; k++) { kids[side][k] = make_cs(side); kids[side][k]->m_dirty = k ? 0x01 : 0x40; }
            if (shape & 1) { cs[side]->m_parent = parent[side]; parent[side]->m_firstChild = cs[side]; }
            if (shape & 2) {
                cs[side]->m_firstChild = kids[side][0];
                kids[side][0]->m_parent = cs[side];
                kids[side][0]->m_nextSibling = kids[side][1];
                kids[side][1]->m_parent = cs[side];
            }
            // From another system that has a parent of its own, whose world matrix must be brought up to date first.
            if (shape & 4) { from[side]->m_parent = parent[side]; from[side]->m_dirty = 0x01; parent[side]->m_dirty = 0x01; }
        }
        const auto compare = [&](const char* what) {
            g_recording = false;
            g_ranges[0].clear();
            g_ranges[1].clear();
            for (int side = 0; side < 2; side++) {
                add_range(side, cs[side], sizeof(AtRotTransCoordSys), 1);
                add_range(side, from[side], sizeof(AtRotTransCoordSys), 2);
                add_range(side, parent[side], sizeof(AtRotTransCoordSys), 3);
                add_range(side, kids[side][0], sizeof(AtRotTransCoordSys), 4);
                add_range(side, kids[side][1], sizeof(AtRotTransCoordSys), 5);
            }
            same_cs(cs[0], cs[1], sizeof(AtRotTransCoordSys));
            same_cs(from[0], from[1], sizeof(AtRotTransCoordSys));
            same_cs(parent[0], parent[1], sizeof(AtRotTransCoordSys));
            same_cs(kids[0][0], kids[1][0], sizeof(AtRotTransCoordSys));
            same_cs(kids[0][1], kids[1][1], sizeof(AtRotTransCoordSys));
            compare_side_effects(what);
        };
        for (const unsigned m : {0x2Bu, 0x10u, 0u}) {
            { On on(0); at<FnMask>(0x00032310)(cs[0], nullptr, m); }
            { On on(1); cs[1]->MarkChildrenDirty(m); }
            compare("children marked");
        }
        { On on(0); at<Fn0>(0x00032DF0)(cs[0], nullptr); }
        { On on(1); cs[1]->LocalFromWorld(); }
        compare("world changed");
        const AtVector3 scale = {1.5f, -2.5f, 0.25f};
        for (const AtVector3* s : {(const AtVector3*)nullptr, &scale}) {
            { On on(0); at<FnFrom>(0x00032E50)(cs[0], nullptr, from[0], s); }
            { On on(1); cs[1]->SetWorldFrom(from[1], s); }
            compare("world from another");
            const AtQuaternion q = {0.125f, -0.25f, 0.5f, 0.75f};
            const AtVector3 p = {7.5f, 8.5f, -9.5f};
            { On on(0); at<FnWorld>(0x00032EF0)(cs[0], nullptr, &q, &p, s); }
            { On on(1); cs[1]->SetWorld(&q, &p, s); }
            compare("world given");
        }
    }
    g_hash[0] = g_hash[1] = 0;
}

// ---- the walks --------------------------------------------------------------------------------------------------------------------

// Heaps a graph reads the fields of: zeroed AtHeap-sized blocks, as in oracle_asm.cpp.
static unsigned char g_heap_graph[sizeof(AtHeap)], g_heap_stack[sizeof(AtHeap)], g_heap_state[sizeof(AtHeap)];

static AtSpaceGraph* graph_for(int side) {
    void* p = std::malloc(sizeof(AtSpaceGraph));
    std::memset(p, 0xCD, sizeof(AtSpaceGraph));
    Reader reader("SpaceGraph\n{\nLeafSpace[1]\nNodeLink[64]\nNodeIncidence[192]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { BoundingBox -100.0 100.0 -100.0 100.0 -100.0 100.0 }\n}\n}\n");
    On on(side);
    if (side == 0) {
        at<Fn0>(0x00039FD0)(p, nullptr);
        at<void(__fastcall*)(void*, void*, void*, void*)>(0x0003AC90)(p, nullptr, g_own[0], &reader.tokenizer);
        return (AtSpaceGraph*)p;
    }
    AtSpaceGraph* g = new (p) AtSpaceGraph;
    g->Read(g_own[1], &reader.tokenizer);
    return g;
}

static void same_nodes_of(AtScene* const* s) {
    for (int k = 0; k < kAtActorKinds; k++) {
        if (dword(s[0], kActorFields[k].count) != dword(s[1], kActorFields[k].count)) { eq("count", 0, 1); continue; }
        for (unsigned i = 0; i < dword(s[0], kActorFields[k].count); i++) {
            same_node((char*)raw(s[0], kActorFields[k].array) + i * kSizes[k], (char*)raw(s[1], kActorFields[k].array) + i * kSizes[k], kSizes[k]);
        }
    }
    for (unsigned i = 0; i < s[0]->m_nodeCount && s[0]->m_nodeCount == s[1]->m_nodeCount; i++) same_node(&s[0]->m_nodes[i], &s[1]->m_nodes[i], 0x6C);
    for (unsigned i = 0; i < s[0]->m_modifierNodeCount && s[0]->m_modifierNodeCount == s[1]->m_modifierNodeCount; i++) same_node(&s[0]->m_modifierNodes[i], &s[1]->m_modifierNodes[i], 0x70);
}

// Every link and item of the graph, by place, and what each node of both scenes holds of it.
static void same_graph(const char* what, AtSpaceGraph* const* g, AtScene* const* s, AtScene* const* t) {
    g_recording = false;
    for (int side = 0; side < 2; side++) {
        register_scene(side, s[side]);
        add_range(side, t[side]->m_nodes, (size_t)t[side]->m_nodeCount * 0x6C, 0x308);
        add_range(side, t[side]->m_modifierNodes, (size_t)t[side]->m_modifierNodeCount * 0x70, 0x309);
        for (int k = 0; k < kAtActorKinds; k++) add_range(side, raw(t[side], kActorFields[k].array), (size_t)dword(t[side], kActorFields[k].count) * kSizes[k], 0x310 + k);
        add_range(side, g[side], sizeof(AtSpaceGraph), 60);
        add_range(side, g[side]->m_links, (size_t)g[side]->m_linkCount * sizeof(AtSceneLink), 61);
        add_range(side, g[side]->m_items, (size_t)g[side]->m_itemCount * sizeof(AtSceneLinkItem), 62);
        add_range(side, g[side]->m_leaves, (size_t)g[side]->m_leafCount * sizeof(AtLeafSpace), 63);
    }
    eq("link count", g[0]->m_linkCount, g[1]->m_linkCount);
    eq("item count", g[0]->m_itemCount, g[1]->m_itemCount);
    eq("kind", g[0]->m_kind, g[1]->m_kind);
    same_ptr("free links", g[0]->m_freeLinks, g[1]->m_freeLinks);
    same_ptr("free items", g[0]->m_freeItems, g[1]->m_freeItems);
    for (unsigned k = 0; k < kAtLinkKinds; k++) same_ptr("dirty", g[0]->m_dirty[k], g[1]->m_dirty[k]);
    if (g[0]->m_linkCount == g[1]->m_linkCount) {
        for (unsigned i = 0; i < g[0]->m_linkCount; i++) {
            for (unsigned off = 0; off < sizeof(AtSceneLink); off += 4) {
                if (off >= 4 && off < 0x14) eq("link's word", dword(&g[0]->m_links[i], off), dword(&g[1]->m_links[i], off));
                else eq("link's pointer", canon_known(0, raw(&g[0]->m_links[i], off)), canon_known(1, raw(&g[1]->m_links[i], off)));
            }
        }
    }
    if (g[0]->m_itemCount == g[1]->m_itemCount) {
        for (unsigned i = 0; i < g[0]->m_itemCount; i++) {
            for (unsigned off = 0; off < sizeof(AtSceneLinkItem); off += 4) {
                if (off < 8) eq("item's word", dword(&g[0]->m_items[i], off), dword(&g[1]->m_items[i], off));
                else eq("item's pointer", canon_known(0, raw(&g[0]->m_items[i], off)), canon_known(1, raw(&g[1]->m_items[i], off)));
            }
        }
    }
    for (unsigned k = 0; k < kAtLinkKinds; k++) same_ptr("leaf's first", g[0]->m_leaves[0].m_first[k], g[1]->m_leaves[0].m_first[k]);
    // Every node's four links, through same_node, by place now that the graph is registered.
    same_nodes_of(s);
    same_nodes_of(t);
    compare_side_effects(what);
}

static void walk_case(const std::string& text, bool through_loader) {
    g_context = text;
    std::set<std::string> names, spaces;
    gather(bytes(text), names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    for (int side = 0; side < 2; side++) {
        g_own[side]->m_stackDepth = 2;
        g_own[side]->m_stackSlots = 2;
    }
    const std::vector<unsigned char> data = bytes(text);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = 0xCD;
    // Two scenes from one text, as a list and the list it names would hold them.
    AtScene* s[2][2];
    for (int i = 0; i < 2; i++) { s[0][i] = make_o(0xCD); s[1][i] = make_r(0xCD); }
    for (int i = 0; i < 2; i++) {
        const bool oc = guarded([&] { load_o(s[0][i]); });
        const bool rc = guarded([&] { load_r(s[1][i]); });
        eq("crash loading", oc, rc);
        if (oc || rc) { compare_logs("crash loading"); return; }
    }
    compare_logs("loaded");
    g_hash[0] = g_hash[1] = 0;
    AtSpaceGraph* g[2] = {graph_for(0), graph_for(1)};
    compare_logs("graph read");
    AtScene* first[2] = {s[0][0], s[1][0]};
    AtScene* second[2] = {s[0][1], s[1][1]};
    same_graph("graph made", g, first, second);
    // The loaders: the outer holds the first scene and names the inner, which holds the second.
    unsigned char outer[2][sizeof(AtSceneDatabaseLoader)], inner[2][sizeof(AtSceneDatabaseLoader)];
    for (int side = 0; side < 2; side++) {
        std::memset(outer[side], 0, sizeof outer[side]);
        std::memset(inner[side], 0, sizeof inner[side]);
        AtSceneDatabaseLoader* o = (AtSceneDatabaseLoader*)outer[side];
        AtSceneDatabaseLoader* n = (AtSceneDatabaseLoader*)inner[side];
        o->m_nodes = s[side][0];
        o->m_nodeCount = 1;
        o->m_loaders = n;
        o->m_loaderCount = 1;
        n->m_nodes = s[side][1];
        n->m_nodeCount = 1;
    }
    for (int round = 0; round < 2; round++) {
        if (through_loader) {
            { On on(0); at<FnP>(0x0002ED70)(outer[0], nullptr, g[0]); }
            { On on(1); ((AtSceneDatabaseLoader*)outer[1])->InsertInto(g[1]); }
        } else {
            { On on(0); at<FnP>(0x0003E860)(s[0][0], nullptr, g[0]); }
            { On on(1); s[1][0]->InsertInto(g[1]); }
        }
        same_graph("inserted", g, first, second);
        if (getenv("KND_WALKS")) {
            unsigned n[4] = {0, 0, 0, 0};
            for (unsigned i = 0; i < s[1][0]->m_nodeCount; i++) for (int k = 0; k < 4; k++) n[k] += s[1][0]->m_nodes[i].m_links[k] != nullptr;
            for (unsigned i = 0; i < s[1][0]->m_modifierNodeCount; i++) for (int k = 0; k < 4; k++) n[k] += s[1][0]->m_modifierNodes[i].m_links[k] != nullptr;
            std::printf("walk: %u nodes, %u modifier nodes; links of plain and modifier nodes by kind %u %u %u %u\n", s[1][0]->m_nodeCount, s[1][0]->m_modifierNodeCount, n[0], n[1], n[2], n[3]);
        }
    }
    for (int round = 0; round < 2; round++) {
        if (through_loader) {
            { On on(0); at<FnP>(0x0002EDD0)(outer[0], nullptr, g[0]); }
            { On on(1); ((AtSceneDatabaseLoader*)outer[1])->RemoveFrom(g[1]); }
        } else {
            { On on(0); at<FnP>(0x0003E890)(s[0][0], nullptr, g[0]); }
            { On on(1); s[1][0]->RemoveFrom(g[1]); }
        }
        same_graph("removed", g, first, second);
    }
    // In again, and the scenes destroyed while in: each node takes itself out.
    { On on(0); at<FnP>(0x0002ED70)(outer[0], nullptr, g[0]); }
    { On on(1); ((AtSceneDatabaseLoader*)outer[1])->InsertInto(g[1]); }
    same_graph("in again", g, first, second);
    {
        AtSpaceGraph* h[2] = {graph_for(0), graph_for(1)};
        compare_logs("second graph read");
        g_hash[0] = g_hash[1] = 0;
        { On on(0); at<FnP>(0x0002ED70)(outer[0], nullptr, h[0]); }
        { On on(1); ((AtSceneDatabaseLoader*)outer[1])->InsertInto(h[1]); }
        same_graph("into a second graph", h, first, second);
        { On on(0); at<FnP>(0x0002EDD0)(outer[0], nullptr, h[0]); }
        { On on(1); ((AtSceneDatabaseLoader*)outer[1])->RemoveFrom(h[1]); }
        same_graph("out of a second graph", h, first, second);
        { On on(0); at<Fn0>(0x0003A030)(h[0], nullptr); }
        { On on(1); h[1]->Clear(); }
        compare_side_effects("second graph cleared");
    }
    for (int i = 0; i < 2; i++) destroy(s[0][i], s[1][i], "destroyed while in");
    { On on(0); at<Fn0>(0x0003A030)(g[0], nullptr); }
    { On on(1); g[1]->Clear(); }
    compare_side_effects("graph cleared");
}

static void run_walks() {
    *(void**)(uintptr_t)0x002C63FC = &g_heap_graph;
    AtSpaceGraph::s_heap = &g_heap_graph;
    *(void**)(uintptr_t)0x002C7E04 = &g_heap_stack;
    AtModifierStack::s_heap = &g_heap_stack;
    *(void**)(uintptr_t)0x002C7EB8 = &g_heap_state;
    AtModifierState::s_heap = &g_heap_state;
    // The first Node[n] is the scene's head, the room for nodes; the second is the list.
    const std::string nodes = "RotTransCoordSys[24]\nScaleCoordSys[2]\nMotionState[3]\nNode[12]\n"
        "Node[3]\n{\n Node \"NA\" RotTransCoordSys\n {\n CollisionEnable\n BoundingSphere 1.0 2.0 3.0 0.5\n }\n Node \"NB\"\n {\n LineSegIntersectEnable\n CollisionEnable\n }\n Node \"NC\"\n {\n }\n}\n"
        "ModifierNode[3]\n{\n ModifierNode \"DA\" RotTransCoordSys\n {\n ModificationEnable\n }\n ModifierNode \"DB\"\n {\n }\n ModifierNode \"DC\"\n {\n ModificationEnable\n CollisionEnable\n LineSegIntersectEnable\n }\n}\n";
    const std::string actors =
        "ModelActor[3]\n{\n ModelActor \"MA\" RotTransCoordSys\n {\n DrawingEnable\n CollisionEnable\n }\n ModelActor \"MB\" RotTransCoordSys\n {\n DrawingEnable\n LineSegIntersectEnable\n }\n ModelActor \"MC\" RotTransCoordSys\n {\n DrawingEnable\n CollisionEnable\n LineSegIntersectEnable\n }\n}\n"
        "ParticleSystemActor[1]\n{\n ParticleSystemActor \"PS\" RotTransCoordSys\n {\n Emitter \"Pe\" \"\"\n DrawingEnable\n }\n}\n"
        "WaterActor[1]\n{\n WaterActor \"WA\" RotTransCoordSys\n {\n DrawingEnable\n CollisionEnable\n }\n}\n"
        "LensFlareActor[1]\n{\n LensFlareActor \"LF\" RotTransCoordSys\n {\n DrawingEnable\n LineSegIntersectEnable\n LensFlare \"Fl\" \"\"\n }\n}\n"
        "GroupActor[1]\n{\n GroupActor \"GA\" RotTransCoordSys\n {\n DrawingEnable\n CollisionEnable\n Actor [1]\n {\n \"MC\" \"\"\n }\n }\n}\n"
        "SwitchGroupActor[1]\n{\n SwitchGroupActor \"SA\" RotTransCoordSys\n {\n DrawingEnable\n LineSegIntersectEnable\n }\n}\n"
        "LodGroupActor[1]\n{\n LodGroupActor \"LA\" RotTransCoordSys\n {\n DrawingEnable\n CollisionEnable\n LineSegIntersectEnable\n }\n}\n"
        "SoundNode[1]\n{\n SoundNode \"SN\" RotTransCoordSys\n {\n CollisionEnable\n LineSegIntersectEnable\n }\n}\n";
    for (const bool loader : {false, true}) {
        walk_case(nodes + actors, loader);
        walk_case(nodes, loader);
        walk_case(std::string("Node[1]\n") + actors, loader);
    }
    // Each class alone, drawn, colliding and tested, so that an array left out of a walk shows.
    for (const char* cls : {"ModelActor", "JointedModelActor", "WaterActor", "LensFlareActor", "GroupActor", "SwitchGroupActor", "LodGroupActor"}) {
        const std::string t = one(cls, "RotTransCoordSys", "DrawingEnable\nCollisionEnable\nLineSegIntersectEnable");
        walk_case(t, false);
        const std::string u = one(cls, "RotTransCoordSys", "DrawingEnable");
        walk_case(u, true);
        const std::string v = one(cls, "RotTransCoordSys", "CollisionEnable");
        walk_case(v, false);
        const std::string w = one(cls, "RotTransCoordSys", "LineSegIntersectEnable");
        walk_case(w, true);
    }
    {
        const std::string p = one("ParticleSystemActor", "RotTransCoordSys", "DrawingEnable\nCollisionEnable\nLineSegIntersectEnable");
        walk_case(p, false);
    }
}

// ---- no heap and no memory ----------------------------------------------------------------------------------------------------------

static void run_node_shortages() {
    const std::string group = with_models("GroupActor", "RotTransCoordSys", "Actor [2]\n{\n \"MA\" \"\"\n \"MB\" \"\"\n}");
    const std::string lod = with_models("LodGroupActor", "RotTransCoordSys", "Lod [2]\n{\n \"MA\" \"\" 1.0\n \"MB\" \"\" 2.0\n}");
    const std::string water = one("WaterActor", "RotTransCoordSys", "WaterPatch [1]\n{\n WaterPatch\n {\n Neighbors 1 2 3 4\n }\n}");
    const std::string sound = one("SoundNode", "RotTransCoordSys", "Emitter \"Em\" \"\"\nPlaySegment \"Seg\" \"\" 2");

    set_actor_heaps(nullptr, &g_heap_set, &g_heap_blend);
    run_text(bytes(group), "no heap for the group", 0xCD, true);
    run_text(bytes(lod), "no heap for the level-of-detail group", 0xCD, true);
    set_actor_heaps(&g_heap_actor, &g_heap_set, &g_heap_blend);
    for (int skip = 0; skip < 3; skip++) {
        g_refuse_heap = &g_heap_actor;
        for (const std::string* t : {&group, &lod}) {
            g_refuse_skip[0] = g_refuse_skip[1] = skip;
            run_text(bytes(*t), "no memory for the group after " + std::to_string(skip), 0xCD, true);
        }
    }
    // The token file's heap holds the file and the tokenizer before the patches and the pending segments: refused from each push in turn.
    for (int skip = 0; skip < 8; skip++) {
        g_refuse_heap = &g_heap_file;
        for (const std::string* t : {&water, &sound}) {
            g_refuse_skip[0] = g_refuse_skip[1] = skip;
            run_text(bytes(*t), "no memory on the file's heap after " + std::to_string(skip), 0xCD, true);
        }
    }
    g_refuse_heap = nullptr;
    g_refuse_skip[0] = g_refuse_skip[1] = 0;
    g_fail_alloc = false;
    g_refusing = 0;
}

static void report(const char* what) {
    std::printf("%s done: %llu comparisons, %d differences; %u texts, %u crashed on both sides\n", what, g_checks, g_failures, g_texts, g_text_crashes);
    g_texts = g_text_crashes = 0;
}

static void run_nodes_tests() {
    run_direct(); report("run_direct");
    run_world(); report("run_world");
    run_group_words(); report("run_group_words");
    run_water_words(); report("run_water_words");
    run_flare_words(); report("run_flare_words");
    run_sound_words(); report("run_sound_words");
    run_walks(); report("run_walks");
    run_node_shortages(); report("run_node_shortages");
}

int main(int argc, char** argv) { return oracle_main(argc, argv, run_nodes_tests); }
