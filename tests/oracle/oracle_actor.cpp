// Differential oracle for the actors (Windows x86, local only). See docs/verification.md.
//
//   oracle_actor <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Built on oracle_asn.cpp, whose harness it shares: the real ModelActor and JointedModelActor on both sides, compared word by word down through what they own. Cases: every .ASN file, and text written here for each word and error path of
// those classes; the slots not reached by a file are driven directly.
#define ORACLE_ACTOR
#include "oracle_asn.cpp"

typedef void(__fastcall* FnPv)(void*, void*, void*);

static std::string model(const std::string& header, const std::string& body) {
    return std::string(kHead) + "ModelActor[1]\n{\n ModelActor \"Self\" " + header + "\n {\n" + body + "\n }\n}\n";
}
static std::string jointed(const std::string& header, const std::string& body) {
    return std::string(kHead) + "JointedModelActor[1]\n{\n JointedModelActor \"Self\" " + header + "\n {\n" + body + "\n }\n}\n";
}
static void both(const std::string& header, const std::string& body) {
    for (const std::string& t : {model(header, body), jointed(header, body)}) run_text(bytes(t), t, 0xCD, true);
}

static void run_actor_words() {
    // Words of the base, on both real classes, and the words around them that change what they do.
    static const char* const bodies[] = {
        "", "DrawingEnable", "DrawingDisable", "DrawingEnable\nDrawingDisable", "ModifierReceiver", "ProjectionReceiver", "CollisionEnable\nDrawingEnable\nModifierReceiver\nProjectionReceiver",
        "MatAssignment \"Mat\" \"\"", "MatAssignment \"Mat\" \"Space\"", "MatAssignment \"XGone\" \"\"", "MatAssignment \"Mat\" \"XNone\"",
        "Modifier \"Mod\" \"\"", "Modifier \"Mod\" \"Space\"", "Modifier \"XGone\" \"\"", "Modifier \"Mod\" \"XNone\"",
        "Mesh \"Thing\" \"\"", "Mesh \"Thing\" \"Space\"", "Mesh \"XGone\" \"\"", "Mesh \"Thing\" \"XNone\"",
        "DataLink \"Path\" \"\"", "DataLink \"Path\" \"Space\"", "DataLink \"XGone\" \"\"", "DataLink \"AVeryLongPathThatGoesOnPastEightBytesAndMoreOfThem\\\\x\" \"Space\"",
        "PhysicsActor \"text\"", "PhysicsActorInstance \"text\"", "PhysicsEntitiesFile \"text\"", "AnimationSkeletonFile \"text\"", "RigidBodySkeletonFile \"text\"", "InitWithHavokPosition\nInitWithHavokOrientation", "PhysicsVehicleActor", "BspBoundModifierNode", "Emitter",
        "AnimateAtDrawEnable", "AnimateAtDrawDisable", "AnimateAtDrawEnable\nAnimateAtDrawDisable",
        "Attach \"Target\" \"\" \"Joint\"", "AttachBase \"Target\" \"\"", "Position 1.5 2.5 3.5", "Other", "5",

        "TextureCoordSysDatabase \"Tex\"", "TextureCoordSysDatabase \"XGone\"", "TextureCoordSysDatabase \"Tex\"\nTextureCoordSysDatabase \"Tex\"",
        "CopyTextureCoordSysDatabase \"Tex\"", "CopyTextureCoordSysDatabase \"Texture1\"", "CopyTextureCoordSysDatabase \"Texture2\"", "CopyTextureCoordSysDatabase \"Texture3\"", "CopyTextureCoordSysDatabase \"Texture4\"", "CopyTextureCoordSysDatabase \"Texture5\"",
        "CopyTextureCoordSysDatabase \"XGone\"", "CopyTextureCoordSysDatabase \"Tex\"\nCopyTextureCoordSysDatabase \"Tex\"", "TextureCoordSysDatabase \"Tex\"\nCopyTextureCoordSysDatabase \"Tex\"",
        "ShareTextureCoordSysDatabase \"Target\" \"\"", "ShareTextureCoordSysDatabase \"XGone\" \"\"", "TextureCoordSysDatabase \"Tex\"\nShareTextureCoordSysDatabase \"Target\" \"\"",

        "TextureCoordAnimationChannel[0]\n{\n}", "TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n }\n}",
        "TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n PlayAnimation \"Anim\" \"Anim\" 40\n PlaybackRate 12.5\n }\n}",
        "CopyTextureCoordSysDatabase \"Tex\"\nTextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n PlayAnimation \"Anim\" \"Anim\" 0\n }\n TextureCoordAnimationChannel\n {\n PlaybackRate 7.5\n }\n}",
        "TextureCoordSysDatabase \"Tex\"\nTextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}",
        "TextureCoordSysDatabase \"Texture1\"\nTextureCoordAnimationChannel[3]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}",
        "TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n }\n}\nTextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n }\n}",
        "TextureCoordAnimationChannel[1]\n{\n AnimationChannel\n {\n }\n}", "TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n }\n}", "TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n }\n",

        "MatAnimationChannel[0]\n{\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}",
        "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Anim\" \"\" 50\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Anim\" \"Space\" 50\n }\n}",
        "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Anim\" \"\" 3\n PlaybackRate 4.5\n MatAssignmentOffset 6\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlaybackRate 4.5\n }\n}",
        "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n MatAssignmentOffset 6\n PlaybackRate 1.5\n PlaybackRate 2.5\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"XGone\" \"\" 3\n }\n}",
        "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Zero\" \"\" 3\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n Other\n }\n}",
        "MatAnimationChannel[2]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Anim\" \"\" 1\n }\n MatAnimationChannel\n {\n PlayMatAnimation \"Other\" \"\" 2\n MatAssignmentOffset 7\n }\n}",
        "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}\nMatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}", "MatAnimationChannel[1]\n{\n Other\n {\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n }\n}", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n",
    };
    for (const char* b : bodies) {
        both("RotTransCoordSys MotionState", b);
        both("", b);
    }
    both("RotTransCoordSys", "Target");
    // What a channel takes. In both lists: the words are one reader's, and the word each starts with differs.
    static const char* const channels[] = {
        "", "PlayAnimation \"Anim\" \"Anim\" 0", "PlayAnimation \"Anim\" \"Anim\" 123", "PlayAnimation \"Anim\" \"Anim\" -5", "PlayAnimation \"XGone\" \"Anim\" 1", "PlayAnimation \"Anim\" \"YGone\" 1", "PlayAnimation \"Zero\" \"Zero\" 1", "PlayAnimation \"Anim\" \"Anim\" x",
        "PlaybackRate 2.5", "PlayAnimation \"Anim\" \"Anim\" 7\nPlaybackRate 2.5", "PlaybackRate 2.5\nPlayAnimation \"Anim\" \"Anim\" 7", "PlayAnimation \"Anim\" \"Anim\" 7\nPlaybackRate 1.5\nPlaybackRate 3.5", "PlaybackRate 0", "PlayAnimation \"Zero\" \"Zero\" 0",
        "Weights \"Anim\" \"Anim\"", "Weights \"XGone\" \"Anim\"", "Weights \"Anim\" \"YGone\"", "WeightMultiplier 0.75", "WeightMultiplier 0.75\nWeightMultiplier 1.25", "LinearVelocity", "AngularVelocity", "LinearVelocity\nAngularVelocity",
        "CoordSysMirrorDatabase \"Anim\"", "CoordSysMirrorDatabase \"XGone\"", "CoordSysMirroringEnable", "CoordSysMirroringDisable", "CoordSysMirroringEnable\nCoordSysMirroringDisable", "LinearVelocity\nCoordSysMirroringEnable\nCoordSysMirroringDisable",
        "ScaleTranslations 1.5 2.5 3.5", "ScaleTranslations 1.5 2.5 x", "Other", "5", "Mesh \"Anim\" \"\"",
    };
    for (const char* c : channels) {
        const std::string one = std::string("TextureCoordAnimationChannel[1]\n{\n TextureCoordAnimationChannel\n {\n") + c + "\n }\n}";
        both("RotTransCoordSys", one);
        const std::string two = std::string("AnimationChannel[1]\n{\n AnimationChannel\n {\n") + c + "\n }\n}";
        const std::string t = jointed("RotTransCoordSys", two);
        run_text(bytes(t), t, 0xCD, true);
    }

    // The jointed class's words.
    static const char* const joints[] = {
        "CoordSysDatabase \"Joints\"", "CoordSysDatabase \"Joints1\"", "CoordSysDatabase \"Joints2\"", "CoordSysDatabase \"Joints3\"", "CoordSysDatabase \"Joints4\"", "CoordSysDatabase \"XGone\"", "CoordSysDatabase \"Joints\"\nCoordSysDatabase \"Joints\"",
        "CopyCoordSysDatabase \"Joints\"", "CopyCoordSysDatabase \"Joints1\"", "CopyCoordSysDatabase \"Joints2\"", "CopyCoordSysDatabase \"Joints3\"", "CopyCoordSysDatabase \"Joints4\"", "CopyCoordSysDatabase \"Joints5\"", "CopyCoordSysDatabase \"Joints6\"", "CopyCoordSysDatabase \"XGone\"",
        "CopyCoordSysDatabase \"Joints\"\nCopyCoordSysDatabase \"Joints\"", "CoordSysDatabase \"Joints\"\nCopyCoordSysDatabase \"Joints\"",
        "ShareCoordSysDatabase \"Target\" \"\"", "ShareCoordSysDatabase \"Target\" \"Space\"", "ShareCoordSysDatabase \"XGone\" \"\"", "ShareCoordSysDatabase \"PlainOne\" \"\"", "CoordSysDatabase \"Joints\"\nShareCoordSysDatabase \"Target\" \"\"", "AttachBase \"Target\" \"\"\nShareCoordSysDatabase \"Target\" \"\"",
        "ShareCoordSysDatabase \"Target\" \"\"\nAttach \"Target\" \"\" \"Joint\"", "ShareCoordSysDatabase \"Target\" \"\"\nAttachBase \"Target\" \"\"",
        "AnimationChannel[0]\n{\n}", "AnimationChannel[1]\n{\n AnimationChannel\n {\n }\n}", "CoordSysDatabase \"Joints\"\nAnimationChannel[2]\n{\n AnimationChannel\n {\n }\n AnimationChannel\n {\n }\n}", "AnimationChannel[2]\n{\n AnimationChannel\n {\n }\n AnimationChannel\n {\n }\n}\nCopyCoordSysDatabase \"Joints\"",
        "CopyCoordSysDatabase \"Joints\"\nAnimationChannel[2]\n{\n AnimationChannel\n {\n PlayAnimation \"Anim\" \"Anim\" 3\n }\n AnimationChannel\n {\n PlayAnimation \"Other\" \"Other\" 4\n }\n}", "AnimationChannel[1]\n{\n AnimationChannel\n {\n }\n}\nAnimationChannel[1]\n{\n AnimationChannel\n {\n }\n}",
        "Mesh \"Thing\" \"\"\nCopyCoordSysDatabase \"Joints\"\nCopyTextureCoordSysDatabase \"Tex\"",
    };
    for (const char* j : joints) {
        for (const char* h : {"RotTransCoordSys MotionState", "RotTransCoordSys ScaleCoordSys", "ScaleCoordSys", ""}) {
            const std::string t = jointed(h, j);
            run_text(bytes(t), t, 0xCD, true);
        }
    }
    // Words not its own: on a model, the jointed class's are the base node's to refuse. The harness's "Target" is a jointed actor of its own; "PlainOne" is not.
    for (const char* w : {"CoordSysDatabase \"Joints\"", "CopyCoordSysDatabase \"Joints\"", "ShareCoordSysDatabase \"Target\" \"\"", "AnimationChannel[1]\n{\n AnimationChannel\n {\n }\n}"}) {
        const std::string t = model("RotTransCoordSys", w);
        run_text(bytes(t), t, 0xCD, true);
    }

    // Scale, every order of the three, and what a sphere then comes to; with and without a rotation above it.
    for (const char* s : {"1.5 2.5 3.5", "1.5 3.5 2.5", "2.5 1.5 3.5", "2.5 3.5 1.5", "3.5 1.5 2.5", "3.5 2.5 1.5", "2.5 2.5 1.5", "1.5 2.5 2.5", "2.5 1.5 2.5", "2.5 2.5 2.5", "-1.5 -2.5 -3.5", "0.0 0.0 0.0"}) {
        for (const char* h : {"RotTransCoordSys ScaleCoordSys", "ScaleCoordSys"}) {
            both(h, std::string("Scale ") + s + "\nBoundingSphere 1.5 2.5 3.5 4.5");
            both(h, std::string("BoundingSphere 1.5 2.5 3.5 4.5\nScale ") + s);
        }
    }
    both("RotTransCoordSys", "Scale 1.5 2.5 3.5");
    both("", "Scale 1.5 2.5 3.5");
    both("RotTransCoordSys ScaleCoordSys ScaleCoordSys", "");
    both("ScaleCoordSys RotTransCoordSys", "BoundingSphere 1.5 2.5 3.5 4.5");
    both("RotTransCoordSys", "BoundingSphere 1.5 2.5 3.5 4.5");
    both("", "BoundingSphere 1.5 2.5 3.5 4.5");
    both("Other", "");
    // Attaching across a scale, and to one with a scale.
    for (const std::string& t : {
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys ScaleCoordSys\n {\n }\n ModelActor \"b\" RotTransCoordSys\n {\n AttachBase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[2]\n{\n JointedModelActor \"a\" RotTransCoordSys ScaleCoordSys\n {\n CopyCoordSysDatabase \"Joints\"\n }\n JointedModelActor \"b\" RotTransCoordSys\n {\n Attach \"a\" \"\" \"Joint\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[2]\n{\n JointedModelActor \"a\" RotTransCoordSys\n {\n CopyCoordSysDatabase \"Joints\"\n AnimationChannel[2]\n {\n  AnimationChannel\n  {\n  }\n  AnimationChannel\n  {\n  }\n }\n }\n JointedModelActor \"b\" RotTransCoordSys MotionState\n {\n ShareCoordSysDatabase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys\n {\n CopyTextureCoordSysDatabase \"Tex\"\n }\n ModelActor \"b\"\n {\n ShareTextureCoordSysDatabase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "Node[1]\n{\n Node \"n\" RotTransCoordSys\n {\n }\n}\nModelActor[1]\n{\n ModelActor \"b\"\n {\n ShareTextureCoordSysDatabase \"n\" \"\"\n }\n}\n",
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\"\n {\n }\n ModelActor \"b\"\n {\n ShareTextureCoordSysDatabase \"a\" \"\"\n }\n}\n",
         }) {
        run_text(bytes(t), t, 0xCD, true);
    }
    // Moved before the sphere is finished, and through a scale.
    both("RotTransCoordSys ScaleCoordSys", "AttachBase \"Target\" \"\"\nScale 1.5 2.5 3.5");
    both("RotTransCoordSys", "AttachBase \"Target\" \"\"");
    // A blend with no database to size it.
    both("RotTransCoordSys", "TextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}");
    {
        const std::string t = jointed("RotTransCoordSys", "AnimationChannel[2]\n{\n AnimationChannel\n {\n }\n AnimationChannel\n {\n }\n}");
        run_text(bytes(t), t, 0xCD, true);
    }
    // Sharing: within the file, a name that is elsewhere under a name space, from what has no database, and several sharing one.
    for (const std::string& t : {
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys\n {\n CopyTextureCoordSysDatabase \"Tex\"\n }\n ModelActor \"b\"\n {\n ShareTextureCoordSysDatabase \"a\" \"Space\"\n }\n}\n",
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys\n {\n TextureCoordAnimationChannel[1]\n {\n TextureCoordAnimationChannel\n {\n }\n }\n }\n ModelActor \"b\"\n {\n ShareTextureCoordSysDatabase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[2]\n{\n JointedModelActor \"a\" RotTransCoordSys\n {\n CopyCoordSysDatabase \"Joints1\"\n }\n JointedModelActor \"b\"\n {\n ShareCoordSysDatabase \"a\" \"Space\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[2]\n{\n JointedModelActor \"a\" RotTransCoordSys\n {\n }\n JointedModelActor \"b\"\n {\n ShareCoordSysDatabase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[3]\n{\n JointedModelActor \"a\" RotTransCoordSys\n {\n CopyCoordSysDatabase \"Joints1\"\n }\n JointedModelActor \"b\"\n {\n ShareCoordSysDatabase \"a\" \"\"\n }\n JointedModelActor \"c\"\n {\n ShareCoordSysDatabase \"a\" \"\"\n }\n}\n",
         }) {
        run_text(bytes(t), t, 0xCD, true);
    }
    // Under other fills, so that nothing rests on what the heap held.
    const std::string busy = jointed("RotTransCoordSys MotionState ScaleCoordSys", "Mesh \"Thing\" \"\"\nCopyCoordSysDatabase \"Joints\"\nCopyTextureCoordSysDatabase \"Tex\"\nAnimationChannel[2]\n{\n AnimationChannel\n {\n PlayAnimation \"Anim\" \"Anim\" 3\n }\n AnimationChannel\n {\n }\n}\nMatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n PlayMatAnimation \"Anim\" \"\" 3\n }\n}\nTextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}");
    for (unsigned char fill : {0x5A, 0x00, 0xFF}) run_text(bytes(busy), busy, fill, false);
}

static void run_actor_shortages() {
    const std::string busy = jointed("RotTransCoordSys", "CopyCoordSysDatabase \"Joints1\"\nCopyTextureCoordSysDatabase \"Texture1\"\nAnimationChannel[2]\n{\n AnimationChannel\n {\n }\n AnimationChannel\n {\n }\n}\nMatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}\nTextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}");
    const std::string model_busy = model("RotTransCoordSys", "CopyTextureCoordSysDatabase \"Texture1\"\nTextureCoordAnimationChannel[2]\n{\n TextureCoordAnimationChannel\n {\n }\n TextureCoordAnimationChannel\n {\n }\n}\nMatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}");
    int* const heaps[3] = {&g_heap_actor, &g_heap_set, &g_heap_blend};
    for (int which = 0; which < 3; which++) {
        set_actor_heaps(which == 0 ? nullptr : &g_heap_actor, which == 1 ? nullptr : &g_heap_set, which == 2 ? nullptr : &g_heap_blend);
        run_text(bytes(busy), "no heap " + std::to_string(which), 0xCD, true);
        run_text(bytes(model_busy), "no heap, model " + std::to_string(which), 0xCD, true);
        set_actor_heaps(&g_heap_actor, &g_heap_set, &g_heap_blend);
        g_refuse_heap = heaps[which];
        run_text(bytes(busy), "no memory " + std::to_string(which), 0xCD, true);
        run_text(bytes(model_busy), "no memory, model " + std::to_string(which), 0xCD, true);
        g_refuse_heap = nullptr;
        g_fail_alloc = false;
        g_refusing = 0;
    }
    // The list alone, so that nothing on the same heap has failed first.
    g_refuse_heap = &g_heap_actor;
    {
        const std::string t = model("", "MatAnimationChannel[1]\n{\n MatAnimationChannel\n {\n }\n}");
        run_text(bytes(t), "no memory for a list", 0xCD, true);
    }
    g_refuse_heap = nullptr;
    g_fail_alloc = false;
    g_refusing = 0;
    // The database heap, with no coordinate systems of the scene's to fail first, on databases of each shape.
    g_refuse_heap = &g_heap_db;
    for (int i = 0; i < 24; i++) {
        const std::string name = "Joints" + std::to_string(i);
        const std::string t = "Node[1]\nJointedModelActor[1]\n{\n JointedModelActor \"j\"\n {\n CopyCoordSysDatabase \"" + name + "\"\n }\n}\n";
        run_text(bytes(t), "no memory for arrays of a copy, " + name, 0xCD, true);
    }
    g_refuse_heap = nullptr;
    g_fail_alloc = false;
    g_refusing = 0;
    // The database heap, for a copy's arrays.
    set_heaps(&g_heap_scene, &g_heap_node, nullptr);
    run_text(bytes(busy), "no heap for a copy", 0xCD, true);
    set_heaps(&g_heap_scene, &g_heap_node, &g_heap_db);
    g_refuse_heap = &g_heap_db;
    run_text(bytes(busy), "no memory for a copy", 0xCD, true);
    g_refuse_heap = nullptr;
    g_fail_alloc = false;
    g_refusing = 0;
}

static void run_actor_direct() {
    g_context = "actors, directly";
    std::set<std::string> none;
    Env env(none, none);
    const auto make = [&](int side, int kind) {
        char* p = (char*)std::malloc(0x100);
        std::memset(p, 0xCD, 0x100);
        On on(side);
        if (side == 0) at<void*(__fastcall*)(void*, void*)>(kOriginalCtor[kind])(p, nullptr);
        else AtSceneActorsConstruct(kind, p, 1);
        return p;
    };
    // Heaps.
    {
        void* a = (void*)(uintptr_t)0x1111;
        at<void(__cdecl*)(void*)>(0x00055B70)(a);
        AtSceneActor::SetHeap(a);
        eq("actor heap", (uintptr_t)*(void**)(uintptr_t)0x002C7F5C, (uintptr_t)AtSceneActor::s_heap);
        void* b = (void*)(uintptr_t)0x2222;
        at<void(__cdecl*)(void*)>(0x00059A70)(b);
        AtSceneAnimSet::SetHeap(b);
        eq("set heap", (uintptr_t)*(void**)(uintptr_t)0x002C9C0C, (uintptr_t)AtSceneAnimSet::s_heap);
        void* c = (void*)(uintptr_t)0x3333;
        at<void(__cdecl*)(void*)>(0x00033C70)(c);
        AtSceneAnimBlend::SetHeap(c);
        eq("blend heap", (uintptr_t)*(void**)(uintptr_t)0x002C63D8, (uintptr_t)AtSceneAnimBlend::s_heap);
        set_actor_heaps(&g_heap_actor, &g_heap_set, &g_heap_blend);
    }
    // The cast: null, each end of the actors, and past each end.
    for (unsigned cls : {1u, 2u, 13u, 0u, 14u, 15u}) {
        char* o = make(0, kAtActorModel);
        char* r = make(1, kAtActorModel);
        ((AtSceneNode*)o)->m_class = (unsigned short)cls;
        ((AtSceneNode*)r)->m_class = (unsigned short)cls;
        void* x;
        void* y;
        { On on(0); x = at<void*(__cdecl*)(void*)>(0x00055930)(o); }
        { On on(1); y = AtSceneActor::AsActor((AtSceneNode*)r); }
        eq("cast", x == o, y == r);
        compare_side_effects("cast");
    }
    {
        // It reports, then reads the class through the null, and the guard catches that on both sides.
        const bool x = guarded([&] { On on(0); at<void*(__cdecl*)(void*)>(0x00055930)(nullptr); });
        g_recording = false;
        const bool y = guarded([&] { On on(1); AtSceneActor::AsActor(nullptr); });
        g_recording = false;
        eq("cast of null crashes", x, y);
        compare_logs("cast of null");
        g_hash[0] = g_hash[1] = 0;
    }

    // Sharing a coordinate system, and stopping. And what the shared one's owner answers.
    {
        char* so = make(0, kAtActorModel);
        char* sr = make(1, kAtActorModel);
        char* wo = make(0, kAtActorJointedModel);
        char* wr = make(1, kAtActorJointedModel);
        AtCoordSys* co = make_cs(0);
        AtCoordSys* cr = make_cs(1);
        AtCoordSys* mo = make_cs(0);
        AtCoordSys* mr = make_cs(1);
        ((AtSceneActor*)wo)->m_coordSys = co;
        ((AtSceneActor*)wr)->m_coordSys = cr;
        ((AtSceneActor*)so)->m_coordSys = mo;
        ((AtSceneActor*)sr)->m_coordSys = mr;
        ((AtSceneActor*)so)->m_owner = g_own[0];
        ((AtSceneActor*)sr)->m_owner = g_own[1];
        ((AtSceneActor*)so)->m_flags |= AtSceneNode::kOwnsCoordSys;
        ((AtSceneActor*)sr)->m_flags |= AtSceneNode::kOwnsCoordSys;
        AtScaleCoordSys* shared_scale[2] = {(AtScaleCoordSys*)std::malloc(sizeof(AtScaleCoordSys)), (AtScaleCoordSys*)std::malloc(sizeof(AtScaleCoordSys))};
        at<Fn0>(0x00032AE0)(shared_scale[0], nullptr);
        new (shared_scale[1]) AtScaleCoordSys;
        ((AtSceneActor*)wo)->m_scale = shared_scale[0];
        ((AtSceneActor*)wr)->m_scale = shared_scale[1];
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, wo, 0xCC, 700);
        add_range(1, wr, 0xCC, 700);
        add_range(0, co, 0x28, 701);
        add_range(1, cr, 0x28, 701);
        add_range(0, mo, 0x28, 702);
        add_range(1, mr, 0x28, 702);
        add_range(0, shared_scale[0], sizeof(AtScaleCoordSys), 703);
        add_range(1, shared_scale[1], sizeof(AtScaleCoordSys), 703);
        { On on(0); ((AtSceneActor*)so)->ShareCoordSys((AtSceneActor*)wo); }
        { On on(1); ((AtSceneActor*)sr)->ShareCoordSys((AtSceneActor*)wr); }
        same_node(so, sr, 0xA0);
        eq("parent coordinate system", canon(0, ((AtSceneActor*)so)->ParentCoordSys()), canon(1, ((AtSceneActor*)sr)->ParentCoordSys()));
        compare_side_effects("shared");
        { On on(0); ((AtSceneActor*)so)->StopSharingCoordSys(); }
        { On on(1); ((AtSceneActor*)sr)->StopSharingCoordSys(); }
        same_node(so, sr, 0xA0);
        compare_side_effects("stopped sharing");
        // A coordinate system of its own, given up.
        ((AtSceneActor*)so)->m_flags |= AtSceneNode::kOwnsCoordSys;
        ((AtSceneActor*)sr)->m_flags |= AtSceneNode::kOwnsCoordSys;
        // Scale coordinate system of its own, given back on clearing.
        AtScaleCoordSys* po = (AtScaleCoordSys*)std::malloc(sizeof(AtScaleCoordSys));
        AtScaleCoordSys* pr = (AtScaleCoordSys*)std::malloc(sizeof(AtScaleCoordSys));
        at<Fn0>(0x00032AE0)(po, nullptr);
        new (pr) AtScaleCoordSys;
        po->m_local = (AtMatrix*)&g_mats[0][1]; po->m_world = (AtMatrix*)&g_mats[0][2];
        pr->m_local = (AtMatrix*)&g_mats[1][1]; pr->m_world = (AtMatrix*)&g_mats[1][2];
        ((AtSceneActor*)so)->m_scale = po;
        ((AtSceneActor*)sr)->m_scale = pr;
        ((AtSceneActor*)so)->m_coordSys = nullptr;
        ((AtSceneActor*)sr)->m_coordSys = nullptr;
        ((AtSceneActor*)so)->m_flags = AtSceneActor::kOwnsScaleCoordSys;
        ((AtSceneActor*)sr)->m_flags = AtSceneActor::kOwnsScaleCoordSys;
        ((AtSceneModelActor*)so)->m_unknown9C = 5;
        ((AtSceneModelActor*)sr)->m_unknown9C = 5;
        ((AtSceneActor*)so)->m_unknown6C = 5;
        ((AtSceneActor*)sr)->m_unknown6C = 5;
        ((AtSceneActor*)so)->m_modifier = (void*)(uintptr_t)5;
        ((AtSceneActor*)sr)->m_modifier = (void*)(uintptr_t)5;
        ((AtSceneActor*)so)->m_dataLink = (void*)(uintptr_t)6;
        ((AtSceneActor*)sr)->m_dataLink = (void*)(uintptr_t)6;
        ((AtSceneActor*)so)->m_matAssignment = (void*)(uintptr_t)7;
        ((AtSceneActor*)sr)->m_matAssignment = (void*)(uintptr_t)7;
        ((AtSceneModelActor*)so)->m_mesh = (void*)(uintptr_t)8;
        ((AtSceneModelActor*)sr)->m_mesh = (void*)(uintptr_t)8;
        { On on(0); ((AtSceneActor*)so)->Clear(); }
        { On on(1); ((AtSceneActor*)sr)->Clear(); }
        same_node(so, sr, 0xA0);
        compare_side_effects("cleared with a scale of its own");
    }
    // The base's deleting destructor, which no array reaches: the base is abstract, so here a double of it on the recovered side.
    {
        char* so = (char*)std::malloc(0x98);
        char* keep = (char*)std::malloc(0x98);
        at<Fn0>(0x00055DB0)(so, nullptr);
        at<Fn0>(0x00055DB0)(keep, nullptr);
        TestActor* sr = new TestActor;
        TestActor* keepr = new TestActor;
        { On on(0); eq("deleter returns this", (uintptr_t)at<FnDel>(0x00056D80)(so, nullptr, 1), (uintptr_t)so); }
        { On on(1); delete sr; }
        compare_side_effects("deleted");
        { On on(0); eq("deleter returns this, not freeing", (uintptr_t)at<FnDel>(0x00056D80)(keep, nullptr, 0), (uintptr_t)keep); }
        { On on(1); keepr->~TestActor(); }
        compare_side_effects("destroyed, not freed");
    }
    // Setting a mesh, a jointed actor's own clearing with what it may hold, and the modifier set's kind 2.
    {
        char* so = make(0, kAtActorModel);
        char* sr = make(1, kAtActorModel);
        { On on(0); ((AtSceneModelActor*)so)->SetMesh((void*)(uintptr_t)0x77); }
        { On on(1); ((AtSceneModelActor*)sr)->SetMesh((void*)(uintptr_t)0x77); }
        same_node(so, sr, 0xA0);

        static AtSceneModifierSet sets[2];
        static AtSceneDirtyOwner lists[2];
        static AtSceneLink links[2];
        static AtSceneLinkItem items[2];
        std::memset(sets, 0, sizeof sets);
        std::memset(lists, 0, sizeof lists);
        std::memset(links, 0, sizeof links);
        std::memset(items, 0, sizeof items);
        for (int side = 0; side < 2; side++) {
            char* a = side ? sr : so;
            items[side].m_list = &lists[side];
            items[side].m_node = (AtSceneNode*)a;
            links[side].m_set = &sets[side];
            links[side].m_items = &items[side];
            sets[side].m_dirty[2] = &links[side];
            ((AtSceneNode*)a)->m_links[2] = &links[side];
            ((AtSceneNode*)a)->m_flags = AtSceneNode::kInDirtyList2;
        }
        const int before = AtSceneModifierSet::s_itemsInUse;
        *(int*)(uintptr_t)0x002C63F4 = before;
        { On on(0); ((AtSceneActor*)so)->Clear(); }
        { On on(1); ((AtSceneActor*)sr)->Clear(); }
        eq("kind 2: the set's kind", sets[0].m_kind, sets[1].m_kind);
        eq("kind 2: free links", sets[0].m_freeLinks == &links[0], sets[1].m_freeLinks == &links[1]);
        eq("kind 2: free items", sets[0].m_freeItems == &items[0], sets[1].m_freeItems == &items[1]);
        eq("kind 2: dirty", sets[0].m_dirty[2] != nullptr, sets[1].m_dirty[2] != nullptr);
        same_node(so, sr, 0xA0);
        compare_side_effects("kind 2");
    }
}

// What a load never shows: fields it has already overwritten, or memory that is freed before it could be compared.
static void run_actor_fields() {
    g_context = "actors, fields";
    std::set<std::string> none;
    Env env(none, none);
    typedef void(__fastcall* FnP)(void*, void*, const void*);
    const auto make = [&](int side, int kind) {
        char* p = (char*)std::malloc(0x100);
        std::memset(p, 0xCD, 0x100);
        On on(side);
        if (side == 0) at<void*(__fastcall*)(void*, void*)>(kOriginalCtor[kind])(p, nullptr);
        else AtSceneActorsConstruct(kind, p, 1);
        return p;
    };
    const auto ranges_clear = [&] { g_ranges[0].clear(); g_ranges[1].clear(); };

    // Finishing a sphere that has moved since the word, with nothing, a rotation, or a scale.
    for (int variant = 0; variant < 3; variant++) {
        AtSceneActor* a[2] = {(AtSceneActor*)make(0, kAtActorModel), (AtSceneActor*)make(1, kAtActorModel)};
        ranges_clear();
        for (int side = 0; side < 2; side++) {
            a[side]->m_owner = g_own[side];
            a[side]->m_flags = AtSceneNode::kMoved | 0x40000000;
            a[side]->m_sphere.center.x = 1.5f; a[side]->m_sphere.center.y = 2.5f; a[side]->m_sphere.center.z = 3.5f; a[side]->m_sphere.radius = 4.5f;
            a[side]->m_worldSphere.center.x = 9; a[side]->m_worldSphere.center.y = 9; a[side]->m_worldSphere.center.z = 9; a[side]->m_worldSphere.radius = 9;
            if (variant == 1) {
                a[side]->m_coordSys = make_cs(side);
                add_range(side, a[side]->m_coordSys, 0x28, 710);
            } else if (variant == 2) {
                AtScaleCoordSys* c = (AtScaleCoordSys*)std::malloc(sizeof(AtScaleCoordSys));
                if (side == 0) at<Fn0>(0x00032AE0)(c, nullptr);
                else new (c) AtScaleCoordSys;
                c->m_local = fresh_matrix(side);
                c->m_world = fresh_matrix(side);
                m_identity(c->m_local);
                m_identity(c->m_world);
                c->m_dirty = 0;
                a[side]->m_scale = c;
                add_range(side, c, sizeof(AtScaleCoordSys), 710);
            }
        }
        { On on(0); a[0]->FinishBody(); }
        { On on(1); a[1]->FinishBody(); }
        same_node(a[0], a[1], 0xA0);
        compare_side_effects("finish body");
    }

    // Clearing, with every field showing that the clearing should reach.
    {
        AtSceneJointedModelActor* j[2] = {(AtSceneJointedModelActor*)make(0, kAtActorJointedModel), (AtSceneJointedModelActor*)make(1, kAtActorJointedModel)};
        for (int side = 0; side < 2; side++) {
            AtSceneJointedModelActor& x = *j[side];
            x.m_owner = g_own[side];
            x.m_unknown6C = 1;
            x.m_matAssignment = (void*)(uintptr_t)2;
            x.m_matChannelCount = 0;
            x.m_modifier = (void*)(uintptr_t)4;
            x.m_sharedFrom = (AtSceneActor*)(uintptr_t)5;
            x.m_dataLink = (void*)(uintptr_t)6;
            x.m_mesh = (void*)(uintptr_t)7;
            x.m_unknown9C = 8;
            x.m_joints.m_database = (AtCoordSysDatabase*)(uintptr_t)9;
            x.m_joints.m_channelCount = 10;
            x.m_joints.m_unknown24 = 11;
            x.m_joints.m_blend.m_unknown14 = 12;
            x.m_jointsFrom = (AtSceneJointedModelActor*)(uintptr_t)13;
            x.m_flags = 0x12345000 & ~(unsigned int)AtSceneJointedModelActor::kOwnsJointDatabase;
        }
        { On on(0); j[0]->Clear(); }
        { On on(1); j[1]->Clear(); }
        ranges_clear();
        same_node(j[0], j[1], 0xCC);
        compare_side_effects("jointed clear");
        // Only the plain actor's array count, which a list of no array can leave.
        AtSceneActor* a[2] = {(AtSceneActor*)make(0, kAtActorModel), (AtSceneActor*)make(1, kAtActorModel)};
        for (int side = 0; side < 2; side++) a[side]->m_matChannelCount = 3;
        { On on(0); a[0]->Clear(); }
        { On on(1); a[1]->Clear(); }
        same_node(a[0], a[1], 0xA0);
        compare_side_effects("count cleared");
    }

    // Playback's small functions, over fields already set.
    {
        AtSceneAnimation anim;
        std::memset(&anim, 0xAB, sizeof anim);
        anim.m_rate = 12.5f;
        AtSceneMatAnimation mat;
        std::memset(&mat, 0xAB, sizeof mat);
        mat.m_rate = 6.5f;
        alignas(4) unsigned char a[2][sizeof(AtSceneAnimPlayback)];
        at<Fn0>(0x0005FD30)(a[0], nullptr);
        new (a[1]) AtSceneAnimPlayback;
        for (int side = 0; side < 2; side++) {
            AtSceneAnimPlayback* x = (AtSceneAnimPlayback*)a[side];
            x->m_flags = 9; x->m_time = 5; x->m_rate = 6; x->m_speed = 7; x->m_translationScale.x = 8;
        }
        at<FnP>(0x0005FD60)(a[0], nullptr, &anim);
        ((AtSceneAnimPlayback*)a[1])->SetAnimation(&anim);
        eq("animation playback", std::memcmp(a[0], a[1], sizeof a[0]), 0);
        unsigned char b[2][sizeof(AtSceneMatPlayback)];
        at<Fn0>(0x0005BAA0)(b[0], nullptr);
        new (b[1]) AtSceneMatPlayback;
        for (int side = 0; side < 2; side++) {
            AtSceneMatPlayback* x = (AtSceneMatPlayback*)b[side];
            x->m_flags = 9; x->m_time = 5; x->m_rate = 6;
        }
        at<FnP>(0x0005BAC0)(b[0], nullptr, &mat);
        ((AtSceneMatPlayback*)b[1])->SetAnimation(&mat);
        eq("material playback", std::memcmp(b[0], b[1], sizeof b[0]), 0);
        for (int side = 0; side < 2; side++) {
            AtSceneMatPlayback* x = (AtSceneMatPlayback*)b[side];
            x->m_flags = 9; x->m_time = 5; x->m_rate = 6;
        }
        at<Fn0>(0x0005B710)(b[0], nullptr);
        ((AtScenePlayback*)b[1])->Rewind();
        eq("rewound", std::memcmp(b[0], b[1], sizeof b[0]), 0);
    }

    // Room to blend, over what is there, going through each shape.
    {
        const auto database = [&](int side, unsigned rot, unsigned scaled) {
            AtCoordSysDatabase* d = (AtCoordSysDatabase*)std::malloc(sizeof(AtCoordSysDatabase));
            std::memset(d, 0xCD, sizeof(AtCoordSysDatabase));
            if (side == 0) {
                at<Fn0>(0x000268F0)(d, nullptr);
                at<void(__fastcall*)(void*, void*, void*, unsigned, unsigned, unsigned)>(0x00026920)(d, nullptr, &g_matdev[0], rot, scaled, 0);
            } else {
                new (d) AtCoordSysDatabase;
                d->Allocate(&g_matdev[1], rot, scaled, 0);
            }
            return d;
        };
        AtSceneAnimBlend* b[2] = {(AtSceneAnimBlend*)std::malloc(sizeof(AtSceneAnimBlend)), (AtSceneAnimBlend*)std::malloc(sizeof(AtSceneAnimBlend))};
        at<Fn0>(0x000320A0)(b[0], nullptr);
        new (b[1]) AtSceneAnimBlend;
        static const unsigned shapes[][2] = {{2, 1}, {0, 2}, {1, 0}, {0, 0}, {3, 3}, {3, 3}};
        for (const auto& shape : shapes) {
            AtCoordSysDatabase* d[2] = {database(0, shape[0], shape[1]), database(1, shape[0], shape[1])};
            { On on(0); at<FnP>(0x00034310)(b[0], nullptr, d[0]); }
            { On on(1); b[1]->Allocate(d[1]); }
            eq("blend's rot count", b[0]->m_rotTransCount, b[1]->m_rotTransCount);
            eq("blend's scale count", b[0]->m_scaleCount, b[1]->m_scaleCount);
            eq("blend's arrays", (b[0]->m_rotations != nullptr) * 4 + (b[0]->m_translations != nullptr) * 2 + (b[0]->m_scales != nullptr), (b[1]->m_rotations != nullptr) * 4 + (b[1]->m_translations != nullptr) * 2 + (b[1]->m_scales != nullptr));
            compare_side_effects("blend");
        }
    }

    // A copy of a database: with a next matrix left over, and over a copy already there.
    {
        std::set<std::string> names = {"Joint", "Other", "Third"};
        AtCoordSysDatabase* from[2] = {nullptr, nullptr};
        for (int i = 0; i < 200; i++) {
            const std::string name = "D" + std::to_string(i);
            AtCoordSysDatabase* x = make_database(0, name, names);
            AtCoordSysDatabase* y = make_database(1, name, names);
            from[0] = x;
            from[1] = y;
            if (x->m_rotTransCount >= 2 && x->m_scaledCount == 3) break;
        }
        AtCoordSysDatabase* to[2] = {(AtCoordSysDatabase*)std::malloc(sizeof(AtCoordSysDatabase)), (AtCoordSysDatabase*)std::malloc(sizeof(AtCoordSysDatabase))};
        at<Fn0>(0x000268F0)(to[0], nullptr);
        new (to[1]) AtCoordSysDatabase;
        to[0]->m_nextMatrix = 7;
        to[1]->m_nextMatrix = 7;
        for (int twice = 0; twice < 2; twice++) {
            { On on(0); at<void(__fastcall*)(void*, void*, void*, void*)>(0x00026E50)(to[0], nullptr, &g_matdev[0], from[0]); }
            { On on(1); to[1]->CopyFrom(&g_matdev[1], from[1]); }
            ranges_clear();
            for (int side = 0; side < 2; side++) {
                register_database(side, from[side], 800);
                register_database(side, to[side], 810);
            }
            same_database("copy", to[0], to[1]);
            compare_side_effects("copy of a database");
        }

        // No memory for the copy itself.
        char* a[2] = {make(0, kAtActorModel), make(1, kAtActorModel)};
        ((AtSceneActor*)a[0])->m_owner = g_own[0];
        ((AtSceneActor*)a[1])->m_owner = g_own[1];
        g_fail_alloc = true;
        const bool x = guarded([&] { On on(0); at<void*(__fastcall*)(void*, void*, void*)>(0x00056260)(a[0], nullptr, from[0]); });
        g_recording = false;
        const bool y = guarded([&] { On on(1); ((AtSceneActor*)a[1])->CopyAnimDatabase(from[1]); });
        g_recording = false;
        g_fail_alloc = false;
        eq("no memory for a copy crashes", x, y);
        compare_logs("no memory for a copy");
        g_hash[0] = g_hash[1] = 0;
    }
}

static void run_actor_tests() {
    run_actor_fields(); std::printf("run_actor_fields done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_actor_direct(); std::printf("run_actor_direct done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_actor_words(); std::printf("run_actor_words done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_actor_shortages(); std::printf("run_actor_shortages done: %llu comparisons, %d differences\n", g_checks, g_failures);
}

int main(int argc, char** argv) { return oracle_main(argc, argv, run_actor_tests); }
