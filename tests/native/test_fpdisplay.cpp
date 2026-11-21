// Host test of the Flash player's display manager on any pointer size: variables, contexts, a written level's frames placing and removing nodes, paths, properties, frames' actions and remote calls.
// What is not recovered (the nodes' slots, sounds, a placement's matrix, what LoadMovie loads) is answered here. Not evidence of a byte match; the differential oracle is tests/oracle/oracle_fpdisplay.cpp.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtEmitter.h"
#include "at/AtFlashLevel.h"

// The variables keep their undefined value in raw storage sized by hand (include/at/AtFpDisplay.h).
static_assert(sizeof(AtFpRfcArgs::m_undefined) == sizeof(AtFpActionScriptValue), "AtFpRfcArgs::m_undefined");
static_assert(sizeof(AtFlashLevel::m_player) == sizeof(AtFpSpriteCharacterNode), "AtFlashLevel::m_player");

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {
int g_heap;
std::string g_calls;
AtFlashSpriteTable* g_sprites;

struct Mat { float f[16]; };
class Pool : public AtMatrixPool {
public:
    Mat m[2];
    void Slot0() override {}
    void Reserve(unsigned int) override {}
    void Clear() override {}
    unsigned int Count() override { return 2; }
    AtMatrix* At(unsigned int i) override { return (AtMatrix*)&m[i & 1]; }
};
void Identity(Mat* m) { std::memset(m, 0, sizeof *m); m->f[0] = m->f[5] = m->f[10] = m->f[15] = 1; }
float MGet(const AtMatrix* m, int r, int c) { return ((const Mat*)m)->f[r * 4 + c]; }
void MSet(AtMatrix* m, int r, int c, float v) { ((Mat*)m)->f[r * 4 + c] = v; }
void MPos(const AtMatrix* m, AtVector3* p) { p->x = ((const Mat*)m)->f[12]; p->y = ((const Mat*)m)->f[13]; p->z = ((const Mat*)m)->f[14]; }
void MIdentity(AtMatrix* m) { Identity((Mat*)m); }
void MTranslation(AtMatrix* m, const AtVector3* p) { ((Mat*)m)->f[12] = p->x; ((Mat*)m)->f[13] = p->y; ((Mat*)m)->f[14] = p->z; }

void InitBase(AtFpCharacterNode* n, int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) {
    n->m_kind = kind;
    n->m_character = character;
    n->m_parent = parent;
    n->m_scripts = scripts;
    n->m_placedFrame = frame;
    n->m_state = state;
    n->m_initialized = 1;
    Pool* p = new Pool;
    Identity(&p->m[0]);
    Identity(&p->m[1]);
    n->m_matrices = p;
    g_calls += "init " + std::to_string(kind) + ";";
}
}  // namespace

// What is not recovered, answered here.
void AtFpDisplayManager::UnloadLevel(int n) { g_calls += "unload " + std::to_string(n) + ";"; }
AtFlashLevel* AtFpDisplayManager::FindLoadedLevel(const char*) { return nullptr; }
AtFlashLevel* AtFpDisplayManager::NewLevel(const char* name, int) { g_calls += std::string("new ") + name + ";"; return nullptr; }
void AtFpDisplayManager::AttachLevel(AtFlashLevel*, int n) { g_calls += "attach " + std::to_string(n) + ";"; }
void AtFlashSoundTable::StopAll() { g_calls += "stopall;"; }
void AtFlashSoundTable::Play(int s) { g_calls += "play " + std::to_string(s) + ";"; }
void AtFpCharacterNode::SetMatrix(const float* t) { ((Pool*)m_matrices)->m[0].f[12] = t[4]; ((Pool*)m_matrices)->m[0].f[13] = t[5]; }
AtFpCharacterNode::~AtFpCharacterNode() {}
void AtFpCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) {}
void AtFpCharacterNode::Release() {}
void AtFpCharacterNode::Slot3() {}
void AtFpCharacterNode::Replace(const int*, int) {}
AtFpBaseBranchCharacterNode::~AtFpBaseBranchCharacterNode() {}
void AtFpBaseBranchCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) {}
void AtFpBaseBranchCharacterNode::Release() {}
void AtFpBaseBranchCharacterNode::Slot3() {}
AtFpSpriteCharacterNode::~AtFpSpriteCharacterNode() {}
void AtFpSpriteCharacterNode::Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) {
    InitBase(this, kind, character, parent, scripts, frame, state);
    m_playing = 1;
    m_frame = -1;
    m_variables.m_heap = &g_heap;
    m_variables.Init();
    m_spriteInitialized = 1;
}
void AtFpSpriteCharacterNode::Release() {}
void AtFpSpriteCharacterNode::Update() { g_calls += "update;"; }
void AtFpSpriteCharacterNode::Slot5() {}
void AtFpSpriteCharacterNode::Acquire() {
    m_sprite = (AtFpSprite*)g_sprites->m_items[m_character];
    for (unsigned i = 0; i < m_sprite->m_refCount; i++) m_sprite->m_refs[i].m_frameData = (AtFlashFrame*)g_sprites->m_frames.m_items[m_sprite->m_refs[i].m_frame];
    m_unknown34 = 1;
}
void AtFpSpriteCharacterNode::Unacquire() {}
void AtFpSpriteCharacterNode::Slot8() {}
void AtFpSpriteCharacterNode::Replace(const int* kc, int) { g_calls += "replace " + std::to_string(kc[1]) + ";"; }
AtFpShapeCharacterNode::~AtFpShapeCharacterNode() {}
void AtFpShapeCharacterNode::Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) { InitBase(this, kind, character, parent, scripts, frame, state); }
void AtFpShapeCharacterNode::Release() {}
void AtFpShapeCharacterNode::Update() {}
void AtFpShapeCharacterNode::Slot5() {}
void AtFpShapeCharacterNode::Acquire() {}
void AtFpShapeCharacterNode::Unacquire() {}
void AtFpShapeCharacterNode::Slot8() {}
void AtFpShapeCharacterNode::Replace(const int*, int) {}
AtFpTextCharacterNode::~AtFpTextCharacterNode() {}
void AtFpTextCharacterNode::Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) { InitBase(this, kind, character, parent, scripts, frame, state); }
void AtFpTextCharacterNode::Release() {}
void AtFpTextCharacterNode::Update() {}
void AtFpTextCharacterNode::Slot5() {}
void AtFpTextCharacterNode::Acquire() {}
void AtFpTextCharacterNode::Unacquire() {}
void AtFpTextCharacterNode::Slot8() {}
void AtFpTextCharacterNode::Replace(const int*, int) {}

namespace {

AtFpActionScriptValue* Int(int v) {
    AtFpActionScriptValue* x = new AtFpActionScriptValue;
    x->m_heap = &g_heap;
    x->m_type = AtFpActionScriptValue::kTypeInt;
    x->m_int = v;
    return x;
}

void TestVariables() {
    AtFpRfcArgs t;
    CHECK(t.m_capacity == 0 && t.m_count == 0);
    AtFpActionScriptValue out;
    out.m_heap = &g_heap;
    CHECK(t.Get("a", &out) == 0 && out.m_type == AtFpActionScriptValue::kTypeNone);
    t.m_heap = &g_heap;
    t.Init();
    CHECK(t.m_capacity == 10);
    CHECK(std::strcmp(t.Undefined()->m_name, "UNDEFINED") == 0);
    for (int k = 0; k < 130; k++) {
        char n[8];
        std::snprintf(n, sizeof n, (k & 1) ? "V%03d" : "v%03d", (k * 37) % 130);
        t.Add(n, Int(k));
    }
    CHECK(t.m_count == 130);
    // Grown at one slot short of full, by half and at least 50: 10, 60, 110, 165.
    CHECK(t.m_capacity == 165);
    for (int i = 1; i < t.m_count; i++) {
        std::string a = t.m_values[i - 1].m_name, b = t.m_values[i].m_name;
        for (auto& c : a) c = (char)std::tolower((unsigned char)c);
        for (auto& c : b) c = (char)std::tolower((unsigned char)c);
        CHECK(a < b);
    }
    CHECK(t.Get("V037", &out) == 1 && out.m_type == AtFpActionScriptValue::kTypeInt && out.m_int == 1);  // 37 * 1 % 130 = 37, set by k = 1
    t.Add("v037", Int(-5));
    CHECK(t.m_count == 130);
    CHECK(t.Get("V037", &out) == 1 && out.m_int == -5);
    CHECK(t.Find("nothing") == t.Undefined());
    t.Clear();
    CHECK(t.m_values == nullptr && t.m_count == 0 && t.m_heap == nullptr);
}

void TestContexts() {
    AtFpDisplayMgrContextStack s;
    std::memset(&s, 0, sizeof s);
    int a, b;
    s.Push(&a, &b);
    CHECK(s.m_count == 1 && s.GetContext() == (void*)&a && s.GetTarget() == (void*)&b);
    s.SetTarget(&a);
    CHECK(s.GetTarget() == (void*)&a);
    for (int k = 0; k < 49; k++) s.Push(&b, &b);
    CHECK(s.m_count == 50);
}

void TestVectors() {
    AtVector2 v = {3, 4}, w = {0, 0};
    w.SetNormalized(v);
    CHECK(std::fabs(w.x - 0.6f) < 1e-6f && std::fabs(w.y - 0.8f) < 1e-6f);
    CHECK(std::fabs(v.NormalizeLength() - 5.0f) < 1e-6f && std::fabs(v.x - 0.6f) < 1e-6f);
    AtVector2 z = {1e-7f, 0};
    CHECK(z.NormalizeLength() == 0 && z.x == 1 && z.y == 0);
}

// A written level: sprite 0 places a sprite "kid" (sprite 1), a shape and an unnamed sprite, then moves, removes and queues actions; sprite 1 has two frames.
struct Level {
    std::vector<AtFlashTag*> tags[4];
    AtFlashFrame frames[4];
    AtFpSprite sprites[2];
    AtFpSprite::FrameRef refs0[3], refs1[2];
    AtFlashResource* spriteItems[2];
    AtFlashResource* frameItems[4];
    AtFlashSpriteTable table;
    AtFlashEnvironment env;
    AtFlashApplication app;
    AtFlashLevel* level;
    AtFpSpriteCharacterNode* root;
    std::vector<std::vector<unsigned char>> code[2];
    std::vector<unsigned char*> pointers[2];
    AtFpScript scripts[2];
    AtFpScript* scriptList[2];
    AtTextTable strings;
};

AtFpPlaceObject2Tag* Place(int type, unsigned depth, int kind, int character, unsigned flags, const char* name) {
    AtFpPlaceObject2Tag* t = new AtFpPlaceObject2Tag;
    t->m_type = 1;
    t->m_unknown30 = type;
    t->m_unknown28 = (unsigned short)depth;
    t->m_unknown50 = kind;
    t->m_unknown54 = character;
    t->m_unknown34 = (unsigned char)flags;
    t->m_unknown2A = 9;
    const float tr[6] = {1, 0, 0, 1, 25, -4};
    std::memcpy(t->m_transform, tr, sizeof tr);
    if (name) { t->m_name = (char*)std::malloc(std::strlen(name) + 1); std::strcpy(t->m_name, name); }
    return t;
}
template <class T> T* Id(int type, int id) { T* t = new T; t->m_type = type; t->m_id = id; return t; }

// A script: SetVariable of a name to an integer, then End.
void Script(Level& w, int k, const char* name, int value) {
    std::vector<unsigned char> push = {0x96, 0, 0, 0, 0, 0};
    push.insert(push.end(), name, name + std::strlen(name) + 1);
    push.push_back(7); push.push_back(0);
    for (int i = 0; i < 4; i++) push.push_back((unsigned char)(value >> (8 * i)));
    push[2] = (unsigned char)(push.size() - 4);
    w.code[k] = {push, {0x1D, 0}, {0, 0}};
    for (auto& c : w.code[k]) w.pointers[k].push_back(c.data());
    w.scripts[k].m_instructions = w.pointers[k].data();
    w.scripts[k].m_read = w.scripts[k].m_instructionCount = (unsigned)w.pointers[k].size();
    w.scriptList[k] = &w.scripts[k];
}

void TestLevel() {
    Level* w = new Level;
    w->tags[0] = {Place(0, 5, 2, 1, 1 | 4, "kid"), Place(0, 3, 0, 0, 2, nullptr), Place(0, 7, 2, 1, 0, nullptr), Id<AtFlashActionTag>(4, 0)};
    w->tags[1] = {Place(1, 5, 2, 1, 8, nullptr), Id<AtFlashRemoveTag>(2, 3), Id<AtFlashSoundTag>(3, 4), Id<AtFlashActionTag>(4, 1)};
    w->tags[2] = {};
    w->tags[3] = {};
    for (int i = 0; i < 4; i++) { w->frames[i].m_tags = w->tags[i].data(); w->frames[i].m_tagCount = (unsigned)w->tags[i].size(); w->frameItems[i] = &w->frames[i]; }
    w->refs0[0] = {(char*)"Start", 0, 0};
    w->refs0[1] = {nullptr, 1, 0};
    w->refs0[2] = {(char*)"End", 2, 0};
    w->refs1[0] = {(char*)"In", 3, 0};
    w->refs1[1] = {nullptr, 2, 0};
    w->sprites[0].m_refs = w->refs0; w->sprites[0].m_refCount = 3;
    w->sprites[1].m_refs = w->refs1; w->sprites[1].m_refCount = 2;
    w->spriteItems[0] = &w->sprites[0];
    w->spriteItems[1] = &w->sprites[1];
    w->table.m_items = w->spriteItems;
    w->table.m_itemCount = 2;
    w->table.m_frames.m_items = w->frameItems;
    w->table.m_frames.m_itemCount = 4;
    g_sprites = &w->table;
    std::memset(&w->env, 0, sizeof w->env);
    std::memset(&w->app, 0, sizeof w->app);
    w->env.m_application = &w->app;
    w->env.m_actions.m_heap = &g_heap;
    w->level = (AtFlashLevel*)std::calloc(1, sizeof(AtFlashLevel));
    AtFlashLevel* L = w->level;
    new (&L->m_scripts) AtFlashScriptTable;
    Script(*w, 0, "ran", 1);
    Script(*w, 1, "kid:fromroot", 2);
    L->m_scripts.m_scripts = w->scriptList;
    L->m_scripts.m_count = 2;
    L->m_scripts.m_vm.Init(&w->app, &w->strings);
    L->m_environment = &w->env;
    w->root = new (L->m_player) AtFpSpriteCharacterNode;
    L->m_root = w->root;
    w->root->m_heap = &g_heap;
    w->root->Init(2, 0, nullptr, &L->m_scripts, 0, 1);
    w->root->SetName("_level0");
    w->root->Acquire();
    AtFpDisplayManager& d = w->app.m_display;
    d.m_active.m_head = d.m_active.m_tail = &L->m_link;
    d.m_contexts.m_count = 1;
    d.m_contexts.m_entries[0].m_context = d.m_contexts.m_entries[0].m_target = w->root;
    d.m_maxLevel = 4;

    g_calls.clear();
    w->root->GotoFrame(0);
    // Three placed, each initialised, the sprites acquired and updated, by depth; the unnamed sprite named for the level's count; the action queued.
    CHECK(g_calls == "init 2;init 0;init 2;update;update;");
    AtFpCharacterNode* c = w->root->ChildAtDepth(3);
    CHECK(c && c->m_kind == 0 && c->m_colorScale[0] == 1.0f);
    AtFpSpriteCharacterNode* kid = (AtFpSpriteCharacterNode*)w->root->ChildAtDepth(5);
    CHECK(kid && std::strcmp(kid->m_name, "kid") == 0 && kid->m_state == 1 && kid->m_parent == w->root);
    CHECK(((Pool*)kid->m_matrices)->m[0].f[12] == 25.0f);
    AtFpCharacterNode* unnamed = w->root->ChildAtDepth(7);
    CHECK(unnamed && std::strcmp(unnamed->m_name, "instance0") == 0 && L->m_read == 1);
    CHECK(w->root->m_children.m_head == &c->m_link && w->root->m_children.m_tail == &unnamed->m_link);
    CHECK(w->env.m_actions.m_items.m_head != nullptr && ((AtFpActionQueue::Item*)w->env.m_actions.m_items.m_head)->m_node == w->root);

    // Variables by path.
    AtFpActionScriptValue* v = Int(42);
    d.SetVariable("/kid:score", v);
    d.SetVariable("top", Int(7));
    AtFpActionScriptValue out;
    out.m_heap = &g_heap;
    CHECK(d.GetVariable("_level0/kid:score", &out) == 1 && out.m_int == 42);
    CHECK(d.GetVariable("kid:SCORE", &out) == 1 && out.m_int == 42);
    CHECK(d.GetVariable(":top", &out) == 1 && out.m_int == 7);
    CHECK(d.GetVariable("nothing:top", &out) == 0 && out.m_type == AtFpActionScriptValue::kTypeNone);
    CHECK(d.ResolveTarget("/kid") == &kid->m_variables);
    CHECK(d.ResolvePath("/", 0) == w->root);

    // Properties.
    d.GetProperty("/kid", 11, &out);
    CHECK(out.m_type == 0 && std::strcmp(out.m_text, "/kid") == 0);
    d.GetProperty("/kid", 13, &out);
    CHECK(out.m_type == 0 && std::strcmp(out.m_text, "kid") == 0);
    d.GetProperty("/kid", 0, &out);
    CHECK(out.m_type == AtFpActionScriptValue::kTypeInt && out.m_int == 25);
    d.GetProperty("", 5, &out);
    CHECK(out.m_int == 3);
    AtFpActionScriptValue* ninety = Int(90);
    d.SetProperty("/kid", 10, ninety);
    Mat& m = ((Pool*)kid->m_matrices)->m[0];
    CHECK(std::fabs(m.f[0]) < 1e-6f && std::fabs(m.f[1] - 1) < 1e-6f && std::fabs(m.f[4] + 1) < 1e-6f && m.f[12] == 25.0f && kid->m_scripted == 1);
    d.GetProperty("/kid", 10, &out);
    CHECK(std::fabs(out.Real() - 90.0f) < 1e-3f);
    d.SetProperty("/kid", 2, Int(200));
    d.GetProperty("/kid", 2, &out);
    CHECK(std::fabs(out.Real() - 200.0f) < 1e-3f);

    // Targets.
    d.SetTarget("kid");
    CHECK(d.m_contexts.GetTarget() == kid);
    d.SetTarget("nothing");
    CHECK(d.m_contexts.GetTarget() == w->root);

    // The next frame: moves, removes the shape to the dead nodes, plays, queues.
    g_calls.clear();
    d.GotoFrame(1);
    CHECK(w->root->m_frame == 1);
    CHECK(w->root->ChildAtDepth(3) == nullptr && c->m_state == 2 && w->env.m_dead.m_head == &c->m_link);
    CHECK(kid->m_ratio == 9);
    CHECK(g_calls == "play 4;");
    // Labels and expressions, backwards: what was placed after frame 0 goes, nothing was.
    d.GotoFrameByLabel("START");
    CHECK(w->root->m_frame == 0);
    d.GotoFrameByExpression("kid:2");
    CHECK(kid->m_frame == 1);
    d.GotoFrameByExpression("/kid:In");
    CHECK(kid->m_frame == 0);
    CHECK(d.GetCurrentFrame() == 0);

    // Frames' actions run now: frame 1's script sets kid's variable through the root.
    CHECK(d.ExecuteScripts(1) == 0);
    CHECK(d.GetVariable("/kid:fromroot", &out) == 1 && out.m_int == 2);
    CHECK(d.ExecuteScripts("Start") == 0);
    CHECK(d.GetVariable("ran", &out) == 1 && out.m_int == 1);
    CHECK(d.m_contexts.m_count == 1);

    // Sounds and movies.
    g_calls.clear();
    d.StopSounds();
    d.LoadMovie("menu.swf", "_level2");
    d.LoadMovie("", "_level3");
    d.LoadMovie("x", "_level9");
    CHECK(g_calls == "stopall;new menu.swf;attach 2;unload 3;");

    // Removing the kid takes its children too.
    w->root->RemoveChild(kid);
    CHECK(kid->m_state == 2 && w->env.m_dead.m_head == &kid->m_link);
}

std::string g_rfc;
void Translate(const char* name, char* out) { std::strcpy(out, name); }
void Handler(AtFpRfcArgs* args) { g_rfc += "called " + std::to_string(args ? args->m_count : -1) + ";"; }

void TestRfc() {
    AtFpRfcTable t;
    std::memset(&t, 0, sizeof t);
    t.m_translate = Translate;
    AtFpRfcTable::Registered second = {nullptr, Handler, "Other"};
    t.m_first = {&second, Handler, "First"};
    AtFpRfcArgs args;
    args.m_heap = &g_heap;
    args.Init();
    args.Add("a", Int(1));
    t.Call("other", &args);
    t.Call("FIRST", nullptr);
    t.Call("missing", nullptr);
    CHECK(g_rfc == "called 1;called -1;");
    const std::string longName(300, 'f');
    t.Call(longName.c_str(), nullptr);
    CHECK(g_rfc == "called 1;called -1;");
}

}  // namespace

int main() {
    g_atMatrix.GetElement = MGet;
    g_atMatrix.SetElement = MSet;
    g_atMatrix.GetPosition = MPos;
    g_atMatrix.Identity = MIdentity;
    g_atMatrix.SetTranslation = MTranslation;
    AtFpActionScriptValue::s_heap = &g_heap;
    AtFpActionScriptStack::s_heap = &g_heap;
    AtFpActionScriptVM::s_heap = &g_heap;
    AtFpRfcArgs::s_heap = &g_heap;
    AtFpCharacterNode::s_heap = &g_heap;
    AtFpActionQueue::s_heap = &g_heap;
    AtFpRfcTable::s_heap = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;
    TestVariables();
    TestContexts();
    TestVectors();
    TestLevel();
    TestRfc();
    if (g_fail) { std::printf("%d failures\n", g_fail); return 1; }
    std::printf("test_fpdisplay: ok\n");
    return 0;
}
