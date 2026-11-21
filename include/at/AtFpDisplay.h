// The Flash player's display manager, which the action script machine (include/at/AtFlashScript.h) calls out to: its stack of contexts, the levels it shows, the tree of character nodes each level is, each sprite's
// variables, and the remote function calls into game code. Original address clusters 0x0002B970-0x0002B9D8, 0x0002BF20-0x0002CCA1, 0x0002CFD0-0x0002D063 (the manager), 0x00016080-0x00016174, 0x000161B0-0x000161D8,
// 0x000162C0-0x0001636E (its contexts and the queue of actions), 0x00014250-0x000145F5, 0x00014600-0x00014C0F, 0x00014CA0-0x00014D8A, 0x00014F00-0x000152D3 (sprite nodes), 0x00020B60-0x00020BF4, 0x00020D20-0x00020D79,
// 0x00020E10-0x00020E9A, 0x00020F70-0x00020FC4, 0x000210B0-0x000210CC, 0x00021130-0x00021415, 0x000214A0-0x00021605 (nodes), 0x00021620-0x00021E65 (variables), 0x00021F10-0x00021FFC, 0x00030570-0x00030589,
// 0x00016A60-0x00016AD2, 0x00013380-0x000133A1, 0x00012720-0x0001278C, 0x0002D9E0-0x0002DB4E.
//
// AtFpDisplayManager, AtFpDisplayMgrContextStack, AtFpSprite, AtFpSpriteCharacterNode and AtFpBaseBranchCharacterNode are the original's own names, from its messages ("AtFpDisplayManager::vGotoFrameByNumber ..." at
// 0x002745FC, "AtFpDisplayMgrContextStack::poGetCurrentTarget ..." at 0x00271EC8, "AtFpSpriteCharacterNode::vGetTargetPath ..." at 0x00271954, "AtFpBaseBranchCharacterNode::vRemoveChildToDeadNodeList ..." at
// 0x00272A68), and so are the method names where a message gives one; RFC is from "RFC %s not defined. (%s)" at 0x00274888. Everything else is inferred, AtFpCharacterNode and the shape and text nodes among it.
//
// Not recovered, each stopping with a diagnostic (src/at/AtFpDisplaySeams.cpp): what loads, attaches and unloads a level for LoadMovie, the sounds a level plays, a node's matrix as a placement sets it, and every
// virtual slot of the nodes (what initialises, acquires, updates and replaces one): what draws or plays, and what only running a level's frames reaches.
#pragma once

class AtFlashFrame;
class AtFlashLevel;
class AtFlashScriptTable;
class AtFpActionScriptValue;
class AtFpPlaceObject2Tag;
class AtFpSprite;
class AtMatrixPool;

// A link in one of the player's intrusive lists: a node's at its +0x04, a level's at its +0x00.
struct AtFlashLink {
    AtFlashLink* m_next;               // +0x00
    AtFlashLink* m_prev;               // +0x04
};

// The head and tail of such a list. Most of the player's code edits its lists inline; these two are what it calls.
struct AtFlashList {
    void Append(AtFlashLink* link);    // 0x00012720 at the tail
    void Remove(AtFlashLink* link);    // 0x00012750 the link's own pointers are left as they were; a null link faults

    AtFlashLink* m_head;               // +0x00
    AtFlashLink* m_tail;               // +0x04
};

// The variables of a sprite node, sorted by name without case; and, for Trace, the arguments of a remote function call. 0x3C bytes. The name says what the action script machine first showed it doing.
class AtFpRfcArgs {
public:
    // The constructor and destructor are the original's 0x00021620 and 0x00021AD0, as named calls so that a harness can stand in for them.
    AtFpRfcArgs() { Construct(); }
    ~AtFpRfcArgs() { Destruct(); }
    void Construct();                  // 0x00021620
    void Destruct();                   // 0x00021AD0 Clear, then the undefined value's destructor
    void Init();                       // 0x000216E0 room for 10, from m_heap or s_heap; the undefined value is named "UNDEFINED". Does not free what was there.
    void Add(const char* name, AtFpActionScriptValue* value);  // 0x00021DD0 set: a copy, inserted or over the old value
    void Clear();                      // 0x00021680 frees the values; m_heap is cleared as well
    int Get(const char* name, AtFpActionScriptValue* out);     // 0x00021820 a copy; 0 for a name not found, or one whose value is undefined
    AtFpActionScriptValue* Find(const char* name);             // 0x000217B0 by binary search; the undefined value when not found
    void Insert(AtFpActionScriptValue* value);                 // 0x00021860 a copy at its place by name; grows by half, 50 to 100, when one slot is left

    AtFpActionScriptValue* Undefined() { return (AtFpActionScriptValue*)m_undefined; }

    AtFpActionScriptValue* m_values;   // +0x00 an array of m_capacity, made with new[]
    unsigned int m_capacity;           // +0x04 signed in Insert's arithmetic
    int m_count;                       // +0x08
    // +0x0C an AtFpActionScriptValue, constructed by Construct: what Find gives for a name not found. Its class is not complete here; 0x2C bytes on x86, 0x40 where pointers are 8 (checked in tests/native/test_fpdisplay.cpp).
    alignas(void*) char m_undefined[sizeof(void*) == 4 ? 0x2C : 0x40];
    void* m_heap;                      // +0x38 set by the caller before Init

    static void* s_heap;               // 0x002C6320
};

// The application's remote function calls, at its +0x264: by name, into game code. A name is first translated by m_translate, then looked for among the functions registered in the list from m_first, then among the
// table s_entries, which the game fills in at start (0x00200DF3, not recovered).
class AtFpRfcTable {
public:
    struct Registered {
        Registered* m_next;                        // +0x00
        void (*m_function)(AtFpRfcArgs* args);     // +0x04 cdecl
        const char* m_name;                        // +0x08
    };
    // The original's entries are 0x18 bytes: a member function pointer as {address, adjustment of this} at +0x00, the name at +0x10. Here the compiler's own member pointer stands for the first.
    struct Entry {
        void (AtFpRfcTable::*m_method)(AtFpRfcArgs* args);
        const char* m_name;
    };

    void Call(const char* name, AtFpRfcArgs* args);  // 0x0002D9E0 a name not found builds a message and drops it

    char m_unknown00[8];
    void (*m_translate)(const char* name, char* out);  // +0x08 cdecl; out has room for the name's length and one
    Registered m_first;                                // +0x0C nothing is called unless it has a function and a name

    static void* s_heap;               // 0x002C6394 for a translated name of 0x100 bytes or more
    static unsigned int s_count;       // 0x002B1370 1
    static Entry s_entries[1];         // 0x002B1378 "PreloadMovie", its function set by the game
};

class AtFpSpriteCharacterNode;

// At the display manager's +0x38: 50 entries of a context and a target, counted from 1. A context and a target are sprite nodes; the action script machine reads the current entry itself.
class AtFpDisplayMgrContextStack {
public:
    struct Entry {
        void* m_context;
        void* m_target;
    };
    void Push(void* context, void* target);  // 0x00016080 reports a full stack and pushes nothing
    void GetCurrentContext(Entry* out);      // 0x000160E0 reports a count out of 1 to 50, and reads the entry all the same
    void SetTarget(void* target);            // 0x00016130 the same; on the current entry
    AtFpSpriteCharacterNode* GetContext();   // 0x00016180 0 for a count out of range; its message says poGetCurrentTarget too
    AtFpSpriteCharacterNode* GetTarget();    // 0x000161B0 0 for a count out of range

    void* m_unknown000;                // +0x000
    Entry m_entries[50];               // +0x004
    unsigned int m_count;              // +0x194 1 to 50 when valid; popped by the manager itself
};

// What a frame's actions are queued on for the player to run (an environment's +0x1A0), each with the node it is for. 0x18 bytes.
class AtFpActionQueue {
public:
    struct Item {
        AtFlashLink m_link;            // +0x00
        int m_script;                  // +0x08
        AtFpSpriteCharacterNode* m_node;  // +0x0C
    };
    void Push(int script, AtFpSpriteCharacterNode* node);  // 0x000162C0 an item from m_free if there is one, else a new one from m_heap or s_heap

    void* m_unknown00;                 // +0x00
    AtFlashList m_items;               // +0x04
    AtFlashList m_free;                // +0x0C
    void* m_heap;                      // +0x14

    static void* s_heap;               // 0x002C6268
};

// A level's sounds (its +0x190). Not recovered: only what the player calls is declared, and it stops with a diagnostic.
class AtFlashSoundTable {
public:
    void StopAll();                    // 0x00012AA0 not recovered
    void Play(int sound);              // 0x00012790 not recovered
};

// A node of a level's tree: a sprite, a shape or a text, placed at a depth of its parent sprite. 0x58 bytes. Vftable 0x00272974. Only the constructor and what the sprites' code calls directly are recovered; the
// virtual slots are not, and stop with a diagnostic. Slots 4 to 8 are pure in this class.
class AtFpCharacterNode {
public:
    enum Kind { kShape = 0, kSprite = 2, kText = 3 };  // kSprite by the classes made for it; the other two names are guesses from Flash
    enum State { kAlive = 1, kDead = 2 };

    AtFpCharacterNode();               // 0x00020F70 the link is left as it is
    virtual ~AtFpCharacterNode();      // slot 0, 0x00021080 not recovered
    virtual void Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state);  // slot 1, 0x00020D80 not recovered
    virtual void Release();            // slot 2, 0x00020FD0 not recovered; what Init calls when already initialised
    virtual void Slot3();              // slot 3, 0x00020D40 not recovered
    virtual void Update() = 0;         // slot 4: the frame's tick
    virtual void Slot5() = 0;          // slot 5
    virtual void Acquire() = 0;        // slot 6: its resources
    virtual void Unacquire() = 0;      // slot 7
    virtual void Slot8() = 0;          // slot 8
    virtual void Replace(const int* kindAndCharacter, int frame);  // slot 9, 0x00020D50 not recovered

    void SetName(const char* name);    // 0x00020E10 a copy from m_heap or s_heap; the old one freed, not nulled
    AtFpSpriteCharacterNode* Root();   // 0x00020D70 its level's root
    void SetMatrix(const float* transform);  // 0x00020EA0 not recovered: the six numbers of a placement into its first matrix
    AtFlashLevel* Level();             // the level whose scripts m_scripts is: see m_scripts

    AtFlashLink m_link;                // +0x04 in its parent's children, its parent's pending nodes, or the environment's dead nodes
    int m_initialized;                 // +0x0C
    AtMatrixPool* m_matrices;          // +0x10 two, made by Init
    int m_kind;                        // +0x14 -1 until Init
    int m_character;                   // +0x18 -1
    int m_state;                       // +0x1C -1; 1 alive, 2 dead
    int m_scripted;                    // +0x20 set when a script sets a property: placements then leave the matrix alone
    AtFpCharacterNode* m_parent;       // +0x24
    AtFlashScriptTable* m_scripts;     // +0x28 its level's m_scripts: the code reaches the level's fields after it by their offsets from here
    int m_placedFrame;                 // +0x2C -1; compared signed
    unsigned int m_depth;              // +0x30 -1; compared unsigned
    int m_unknown34;                   // +0x34 a sprite's m_acquired
    unsigned char m_colorOffset[4];    // +0x38
    float m_colorScale[4];             // +0x3C 1 each
    char* m_name;                      // +0x4C
    int m_ratio;                       // +0x50 -1
    void* m_heap;                      // +0x54

    static void* s_heap;               // 0x002C631C
};

// What has children. 0x64 bytes. Vftable 0x0027299C.
class AtFpBaseBranchCharacterNode : public AtFpCharacterNode {
public:
    AtFpBaseBranchCharacterNode();     // 0x000210B0
    ~AtFpBaseBranchCharacterNode() override;  // slot 0, 0x00021540 not recovered
    void Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) override;  // slot 1, 0x00030590 not recovered
    void Release() override;           // slot 2, 0x000210D0 not recovered
    void Slot3() override;             // slot 3, 0x00021470 not recovered

    AtFpCharacterNode* ChildAtDepth(unsigned int depth);  // 0x00021210
    void AddChild(AtFpCharacterNode* child);              // 0x00021130 by depth, before any at the same depth; it becomes alive
    void Kill(AtFpCharacterNode* node);                   // 0x000213D0 dead, and first among the environment's dead nodes
    void RemoveAllChildren();                             // 0x000214A0 each killed, a sprite's children first
    void RemoveChild(AtFpCharacterNode* child);           // 0x00021560 the same for one

    int m_branchInitialized;           // +0x58
    AtFlashList m_children;            // +0x5C sorted by depth
};

// A sprite: a timeline of frames and its variables. 0xB8 bytes. Vftable 0x00271994.
class AtFpSpriteCharacterNode : public AtFpBaseBranchCharacterNode {
public:
    AtFpSpriteCharacterNode();         // 0x00014300
    ~AtFpSpriteCharacterNode() override;  // slot 0, 0x000152F0 not recovered
    void Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) override;  // slot 1, 0x000143B0 not recovered
    void Release() override;           // slot 2, 0x00014370 not recovered
    void Update() override;            // slot 4, 0x000153C0 not recovered
    void Slot5() override;             // slot 5, 0x00014420 not recovered
    void Acquire() override;           // slot 6, 0x00014DB0 not recovered: its sprite, from the level's sprites
    void Unacquire() override;         // slot 7, 0x00014DE0 not recovered
    void Slot8() override;             // slot 8, 0x000CBF30 not recovered
    void Replace(const int* kindAndCharacter, int frame) override;  // slot 9, 0x00014C40 not recovered

    void GotoFrame(int frame);         // 0x00015130 runs the tags of each frame up to it; backwards, from the start
    int FindLabel(const char* label);  // 0x00014480 the first frame of that name, without case; -1
    void ExecuteFrame(int frame);      // 0x000145B0 runs a frame's actions now
    void GotoLabel(const char* label);     // 0x0002B9A0
    void ExecuteLabel(const char* label);  // 0x0002B9C0
    AtFpSpriteCharacterNode* FindChild(const char* path, int matchSelf, int* found);  // 0x000144D0 by names separated by '/'
    void TargetPath(char* out, unsigned int size, unsigned int* length);  // 0x00014250 "/name/name", appended at out + *length
    void GetProperty(int index, AtFpActionScriptValue* out);    // 0x00014600 by the Flash numbering
    void SetProperty(int index, AtFpActionScriptValue* value);  // 0x00014910 position, scale and rotation only
    void RemoveChildrenAfter(int frame);   // 0x00014CA0 those placed after it
    void FlushPending();                   // 0x00014D00 each acquired, updated and added
    AtFpCharacterNode* FindPending(unsigned int depth);         // 0x00014D60
    void ExecutePlace(AtFpPlaceObject2Tag* tag);                // 0x00014F00
    AtFpCharacterNode* MakeNode(int kind, unsigned int depth);  // 0x00021240 from m_heap or s_heap

    int m_spriteInitialized;           // +0x64
    int m_playing;                     // +0x68
    int m_frame;                       // +0x6C -1 before the first
    AtFpSprite* m_sprite;              // +0x70
    AtFlashList m_pending;             // +0x74 placed, not yet in the children
    AtFpRfcArgs m_variables;           // +0x7C
};

// Kind 0. 0x64 bytes. Vftable 0x00272ACC.
class AtFpShapeCharacterNode : public AtFpCharacterNode {
public:
    AtFpShapeCharacterNode();          // 0x00021FE0
    ~AtFpShapeCharacterNode() override;  // slot 0, 0x00022280 not recovered
    void Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) override;  // slot 1, 0x00022060 not recovered
    void Release() override;           // slot 2, 0x00022000 not recovered
    void Update() override;            // slot 4, 0x001CC9F0 not recovered
    void Slot5() override;             // slot 5, 0x000220D0 not recovered
    void Acquire() override;           // slot 6, 0x000221D0 not recovered
    void Unacquire() override;         // slot 7, 0x00022200 not recovered
    void Slot8() override;             // slot 8, 0x000CBF30 not recovered
    void Replace(const int* kindAndCharacter, int frame) override;  // slot 9, 0x00021FB0 not recovered

    int m_unknown58;                   // +0x58
    int m_unknown5C;                   // +0x5C
    int m_unknown60;                   // +0x60
};

// Kind 3. 0x60 bytes. Vftable 0x002749D8.
class AtFpTextCharacterNode : public AtFpCharacterNode {
public:
    AtFpTextCharacterNode();           // 0x00030570
    ~AtFpTextCharacterNode() override;  // slot 0, 0x00030FD0 not recovered
    void Init(int kind, int character, AtFpCharacterNode* parent, AtFlashScriptTable* scripts, int frame, int state) override;  // slot 1, 0x00030590 (the branch's, folded) not recovered
    void Release() override;           // slot 2, 0x000305D0 not recovered
    void Update() override;            // slot 4, 0x001CC9F0 not recovered
    void Slot5() override;             // slot 5, 0x000307A0 not recovered
    void Acquire() override;           // slot 6, 0x00030F60 not recovered
    void Unacquire() override;         // slot 7, 0x000306A0 not recovered
    void Slot8() override;             // slot 8, 0x00030600 not recovered
    void Replace(const int* kindAndCharacter, int frame) override;  // slot 9, 0x00021FB0 not recovered

    int m_unknown58;                   // +0x58
    int m_unknown5C;                   // +0x5C
};

class AtFlashEnvironment;

// At the application's +0x004, 0x25C bytes.
class AtFpDisplayManager {
public:
    int GetCurrentFrame();             // 0x0002BF50 the current target's; -1 with none
    void GotoFrame(int frame);         // 0x0002B970
    void SetPlaying(int playing);      // 0x0002BFC0 its message says vGotoFrameByLabel
    void StopSounds();                 // 0x0002BF20 every level's
    void GotoFrameByLabel(const char* label);              // 0x0002BF80
    void GotoFrameByExpression(const char* expression);    // 0x0002CB70 "path:frame", by number from 1 or by label
    void SetTarget(const char* path);                      // 0x0002C3B0 from the current context; the context's root when the path names nothing alive
    void LoadMovie(const char* name, const char* target);  // 0x0002CFD0 target "_levelN"; an empty name unloads level N
    AtFpRfcArgs* ResolveTarget(const char* path);          // 0x0002CC90 the variables of what the path names; not checked for null
    int GetVariable(const char* name, AtFpActionScriptValue* out);    // 0x0002C9C0 "path:name" or a name of the current target
    int SetVariable(const char* name, AtFpActionScriptValue* value);  // 0x0002CAA0 the same; what the original returns is not defined, here 0
    void GetProperty(const char* target, int index, AtFpActionScriptValue* out);    // 0x0002C320
    void SetProperty(const char* target, int index, AtFpActionScriptValue* value);  // 0x0002C360
    int ExecuteScripts(int frame);             // 0x0002C150 not 0 stops the script that called: when its context is gone or dead
    int ExecuteScripts(const char* label);     // 0x0002C1C0 "path:frame" as for GotoFrameByExpression; the same
    AtFpSpriteCharacterNode* ResolvePath(const char* path, int fromContext);  // 0x0002BFF0 "/a/b" from the root, then from each level's root, then from the current context or target

    // Not recovered: what LoadMovie calls.
    void UnloadLevel(int number);                      // 0x0002C570 not recovered
    AtFlashLevel* FindLoadedLevel(const char* name);   // 0x0002C5D0 not recovered
    AtFlashLevel* NewLevel(const char* name, int unused);  // 0x0002CCB0 not recovered
    void AttachLevel(AtFlashLevel* level, int number); // 0x0002C430 not recovered

    char m_unknown000[4];
    AtFlashLevel* m_levels;                    // +0x004 an array of m_levelCount
    AtFlashList m_active;                      // +0x008 the levels shown, by number
    AtFlashList m_loaded;                      // +0x010
    AtFlashList m_unloaded;                    // +0x018
    char m_unknown020[0x18];
    AtFpDisplayMgrContextStack m_contexts;     // +0x038
    char m_unknown1D0[0x4C];
    unsigned int m_levelCount;                 // +0x21C
    unsigned int m_maxLevel;                   // +0x220
    char m_unknown224[0x24];
    AtFlashEnvironment* m_environment;         // +0x248
    char m_unknown24C[0x10];
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtFpRfcArgs) == 0x3C, "AtFpRfcArgs");
static_assert(offsetof(AtFpRfcArgs, m_heap) == 0x38, "AtFpRfcArgs::m_heap");
static_assert(offsetof(AtFpRfcTable, m_first) == 0xC, "AtFpRfcTable::m_first");
static_assert(offsetof(AtFpDisplayMgrContextStack, m_count) == 0x194, "AtFpDisplayMgrContextStack::m_count");
static_assert(sizeof(AtFpActionQueue) == 0x18, "AtFpActionQueue");
static_assert(sizeof(AtFpActionQueue::Item) == 0x10, "AtFpActionQueue::Item");
static_assert(offsetof(AtFpDisplayManager, m_contexts) == 0x38, "AtFpDisplayManager::m_contexts");
static_assert(offsetof(AtFpDisplayManager, m_levelCount) == 0x21C, "AtFpDisplayManager::m_levelCount");
static_assert(offsetof(AtFpDisplayManager, m_environment) == 0x248, "AtFpDisplayManager::m_environment");
static_assert(sizeof(AtFpDisplayManager) == 0x25C, "AtFpDisplayManager");
static_assert(offsetof(AtFpCharacterNode, m_link) == 0x4, "AtFpCharacterNode::m_link");
static_assert(offsetof(AtFpCharacterNode, m_scripts) == 0x28, "AtFpCharacterNode::m_scripts");
static_assert(offsetof(AtFpCharacterNode, m_colorScale) == 0x3C, "AtFpCharacterNode::m_colorScale");
static_assert(offsetof(AtFpCharacterNode, m_heap) == 0x54, "AtFpCharacterNode::m_heap");
static_assert(sizeof(AtFpCharacterNode) == 0x58, "AtFpCharacterNode");
static_assert(offsetof(AtFpBaseBranchCharacterNode, m_children) == 0x5C, "AtFpBaseBranchCharacterNode::m_children");
static_assert(sizeof(AtFpBaseBranchCharacterNode) == 0x64, "AtFpBaseBranchCharacterNode");
static_assert(offsetof(AtFpSpriteCharacterNode, m_frame) == 0x6C, "AtFpSpriteCharacterNode::m_frame");
static_assert(offsetof(AtFpSpriteCharacterNode, m_variables) == 0x7C, "AtFpSpriteCharacterNode::m_variables");
static_assert(sizeof(AtFpSpriteCharacterNode) == 0xB8, "AtFpSpriteCharacterNode");
static_assert(sizeof(AtFpShapeCharacterNode) == 0x64, "AtFpShapeCharacterNode");
static_assert(sizeof(AtFpTextCharacterNode) == 0x60, "AtFpTextCharacterNode");
#pragma pop_macro("offsetof")
#endif
