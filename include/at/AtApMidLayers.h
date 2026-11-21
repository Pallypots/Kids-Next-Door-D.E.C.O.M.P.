// The objects of the four AtAp layers above the I/O layer: math, data management, script and kinematics, and what their Inits bring up: the heaps of
// the classes each layer serves, the data registries' owner, and the small-C script system with its built-in functions. Subsystem at/atlayers.
// Reference build knd-xbox-usa-5454008a-v1.
//
// As for the layers below (include/at/AtApLayer.h), RTTI names sixteen AtAp*Application classes but none of these objects; every class and member name
// here is inferred, from what the objects do and from RTTI's order of the layers. "SmallCScripts" and "SmallCScript" are strings of the executable; the
// built-ins the script system registers (numargs, getarg, strpack, float, floatmul, ...) are those of the Small language's core and float modules.
//
// Each layer's Init takes the config App fills by a slot (include/gm/GmApp.h), the object of the layer below and App's memory system, which none of
// these four reads. A second Init shuts the layer down first.
#pragma once
#include "at/AtDataRegistry.h"
#include "at/AtHeap.h"

class AtIoFileSystem;

// ---- the math layer ---------------------------------------------------------------------------------------------------

// What App's slot 19 fills in (0x10 bytes). Each heap that is 0 falls back to m_heap.
struct AtMathConfig {
    int m_unknown00;                 // +0x00 not read
    AtHeap* m_heap;                  // +0x04
    void* m_segmentHeap;             // +0x08 AtSegmentHitList's
    void* m_queryHeap;               // +0x0C AtQueryPool's
};

// The math layer's object, at +0x384 of App (vftable 0x002847FC, 0x0C bytes).
class AtMathSystem {
public:
    AtMathSystem();                                       // 0x000B38B0
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000B3910 not recovered
    virtual void Init(const AtMathConfig* config, void* below, void* memory);  // slot 1, 0x000B38D0 below is the I/O layer's object
    virtual void Shutdown();                              // slot 2, 0x000E7880 (folded with AtDataComponent::Reset): clears both words
    virtual void SetHeaps(const AtMathConfig* config);    // slot 3, 0x000B3870

    void* m_below;                   // +0x04 set while running
    int m_unknown08;                 // +0x08 cleared, never set here
};

// ---- the data management layer ----------------------------------------------------------------------------------------

// What App's slot 23 fills in (0x34 bytes). Each heap that is 0 falls back to m_heap.
struct AtDataManagementConfig {
    unsigned int m_databaseSpaces;   // +0x00 the owner's namespace database for databases, if not 0
    unsigned int m_registrySpaces;   // +0x04 its namespace database for registries, if not 0
    AtHeap* m_heap;                  // +0x08
    void* m_componentHeap;           // +0x0C AtDataComponent's
    void* m_descriptorHeap;          // +0x10 AtDataDescriptor's
    void* m_managerHeap;             // +0x14 AtDatabaseManager's
    void* m_layoutHeap;              // +0x18 AtRecordLayout's
    void* m_recordDatabaseHeap;      // +0x1C AtDataRecordDatabase's
    void* m_registryHeap;            // +0x20 AtDataRegistry's
    void* m_linkHeap;                // +0x24 AtDataLink's
    void* m_heap28;                  // +0x28 for 0x003ADF80, by 0x000C8960 (read at 0x000C89E4 and 0x000C89FD)
    void* m_nameSpacesHeap;          // +0x2C AtDataRegistryNameSpaces's
    void* m_libraryHeap;             // +0x30 AtDataLibrary's
};
void AtSetHeap3ADF80(void* heap);    // 0x000C8960
extern void* g_heap3ADF80;           // 0x003ADF80

// The data management layer's object, at +0x390 of App (vftable 0x002847EC, 0x84 bytes). Its owner, at App+0x394, is the one the registries and
// libraries App loads are added to (AtDataRegistryOwner, include/at/AtDataRegistry.h).
class AtDataManagementSystem {
public:
    AtDataManagementSystem();                             // 0x000B3770
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000B3850 not recovered
    virtual void Init(const AtDataManagementConfig* config, void* below, void* memory);  // slot 1, 0x000B3790 below is the math layer's object
    virtual void Shutdown();                              // slot 2, 0x000B37D0
    virtual void SetHeaps(const AtDataManagementConfig* config);  // slot 3, 0x000B3690

    AtDataRegistryOwner m_owner;     // +0x04
    void* m_below;                   // +0x80 set while running
};

// ---- the script layer -------------------------------------------------------------------------------------------------

// What App's slot 27 fills in (0x14 bytes). Each heap that is 0 falls back to m_heap.
struct AtScriptConfig {
    int m_unknown00;                 // +0x00 not read
    AtHeap* m_heap;                  // +0x04
    void* m_hostHeap;                // +0x08 AtScriptHost's, which its Init then gives the next two as well
    void* m_factoryHeap;             // +0x0C AtSmallCScriptFactory's
    void* m_systemHeap;              // +0x10 AtSmallCSystem's
};

// A built-in function of the scripts, as the Small language calls them. The machine's callback (0x0007A7D0) calls it with the context registered beside it,
// the abstract machine and the arguments (at/smallvm, include/at/AtSmallVm.h).
struct AtAmx;
typedef int (*AtSmallCNative)(void* context, AtAmx* amx, int* params);

// One built-in, by name (0x1C bytes): both the entries of the tables the system registers from and the entries of its own table.
struct AtSmallCNativeEntry {
    char m_name[0x14];               // +0x00
    AtSmallCNative m_function;       // +0x14 a table ends at the first null
    void* m_context;                 // +0x18 in the system's table, what registered it; 0 in the static tables
};
extern const AtSmallCNativeEntry g_smallCCoreNatives[];   // 0x002B3488 numargs ... toupper
extern const AtSmallCNativeEntry g_smallCFloatNatives[];  // 0x002B3370 float ... floatcmp

// Their functions, not recovered: each stops with a diagnostic (src/at/AtApMidLayersSeams.cpp).
int AtSmallCNumArgs(void* context, AtAmx* amx, int* params);     // 0x0007CA10
int AtSmallCGetArg(void* context, AtAmx* amx, int* params);      // 0x0007CA30
int AtSmallCSetArg(void* context, AtAmx* amx, int* params);      // 0x0007CA60
int AtSmallCHeapSpace(void* context, AtAmx* amx, int* params);   // 0x0007CAB0
int AtSmallCFuncIdx(void* context, AtAmx* amx, int* params);     // 0x0007CAC0
int AtSmallCStrLen(void* context, AtAmx* amx, int* params);      // 0x0007CC30
int AtSmallCStrPack(void* context, AtAmx* amx, int* params);     // 0x0007CC70
int AtSmallCStrUnpack(void* context, AtAmx* amx, int* params);   // 0x0007CD10
int AtSmallCToLower(void* context, AtAmx* amx, int* params);     // 0x0007CDB0
int AtSmallCToUpper(void* context, AtAmx* amx, int* params);     // 0x0007CDD0
int AtSmallCFloat(void* context, AtAmx* amx, int* params);       // 0x0007C800
int AtSmallCFloatStr(void* context, AtAmx* amx, int* params);    // 0x0007C810
int AtSmallCFloatMul(void* context, AtAmx* amx, int* params);    // 0x0007C870
int AtSmallCFloatDiv(void* context, AtAmx* amx, int* params);    // 0x0007C8A0
int AtSmallCFloatAdd(void* context, AtAmx* amx, int* params);    // 0x0007C8D0
int AtSmallCFloatSub(void* context, AtAmx* amx, int* params);    // 0x0007C900
int AtSmallCFloatFract(void* context, AtAmx* amx, int* params);  // 0x0007C930
int AtSmallCFloatRound(void* context, AtAmx* amx, int* params);  // 0x0007C960
int AtSmallCFloatCmp(void* context, AtAmx* amx, int* params);    // 0x0007C9D0

// What the system hands its scripts back to. No fields and no vftable: its constructor (0x00057E20, folded) and its Init and Shutdown (0x001CC9F0,
// folded) do nothing. Its other members (0x0006F5A0 and on) are not recovered.
class AtSmallCScriptFactory {
public:
    void Init() {}                   // 0x001CC9F0 empty (folded)
    void Shutdown() {}               // 0x001CC9F0 empty (folded)
    struct AtSmallCScript* Create();  // 0x0006F5A0 at/smallvm: a new script on s_heap, owned by this
    void Release(void* script);      // 0x0006F640 at/smallvm: resets the script (even a null one), destroys and frees it

    static void SetHeap(void* heap);  // 0x0006F590
    static void* s_heap;             // 0x002CD0E8
};

// The script system (0x5B48 bytes): the table of built-ins, the scripts it holds, and a name map. One instance, s_instance, set by Init; 0x0007AF10
// makes one on s_heap when there is none.
class AtSmallCSystem {
public:
    enum { kMaxNatives = 0x200, kMaxScripts = 2000 };

    AtSmallCSystem();                // 0x000756C0
    void Clear();                    // 0x00075590 also clears s_heap
    void Init();                     // 0x000756E0 the core built-ins, then the float ones, then the name map for 2000
    void Shutdown();                 // 0x000757A0 when running: empties the name map, hands each script back, then as Clear
    int Register(void* context, const char* name, AtSmallCNative function);  // 0x000754D0 0 when the table is full; the name is copied unbounded

    // at/smallvm (src/at/AtSmallCScripts.cpp). The ones taking a name compare at most eight bytes.
    static AtSmallCSystem* Instance();  // 0x0007AF10 makes one on s_heap when there is none
    int FindNative(const char* name);   // 0x00075530 the index in m_natives, or -1
    int Unregister(const char* name);   // 0x00075760 1 if it was there
    unsigned char* LoadProgram(AtAmx* amx, const char* file, int unused);  // 0x000758B0 "<file>.ASL", at most eight characters of file; null on failure
    static int Run(AtSmallCSystem* system, struct AtSmallCScript* script, int numparams, const int* params);  // 0x00075860 cdecl; always 1
    int CallPublic(struct AtSmallCScript* script, int index, int copy);  // 0x00075B20 copy: on a copy of the machine, sharing its memory
    int StartScript(const struct AtSmallCScriptDesc* desc);  // 0x00075CB0 0 when started
    int FindScriptIndex(const char* name);   // 0x00075600 by the script's own name; -1
    struct AtSmallCScript* FindScript(const char* name);  // 0x00075650 through the name map
    int StopScript(const char* name);        // 0x00075660 1 if it was running
    int Activate(const char* name);          // 0x00075A90 not while in a call
    int Cancel(const char* name);            // 0x00075AE0 abandons a suspended call

    AtSmallCNativeEntry m_natives[kMaxNatives];  // +0x0000
    struct AtSmallCHostVar* HostVars() { return (struct AtSmallCHostVar*)m_hostVars; }
    unsigned char m_hostVars[0x3C0]; // +0x3800 40 host variables (AtSmallCHostVar); what fills them is not known
    void* m_scripts[kMaxScripts];    // +0x3BC0 AtSmallCScript
    unsigned int m_nativeCount;      // +0x5B00
    int m_unknown5B04;               // +0x5B04 not touched here
    int m_scriptCount;               // +0x5B08
    int m_lastStarted;               // +0x5B0C the slot StartScript filled last
    int m_unknown5B10;               // +0x5B10
    AtSmallCScriptFactory* m_factory;  // +0x5B14 set by the host
    int m_unknown5B18;               // +0x5B18
    GmNameMap m_names;               // +0x5B1C
    int m_unknown5B30;               // +0x5B30
    AtIoFileSystem* m_programFs;     // +0x5B34 where LoadProgram opens; null: every file system
    int m_stepMode;                  // +0x5B38 while debugging, each LINE suspends
    int m_unknown5B3C;               // +0x5B3C 4 after Clear
    int m_unknown5B40;               // +0x5B40 not touched here
    int m_running;                   // +0x5B44

    static void SetHeap(void* heap);  // 0x00075400
    static void* s_heap;             // 0x002CF170
    static AtSmallCSystem* s_instance;  // 0x002CF174
};

// What the script layer runs: the factory and the system (0x5B50 bytes, at App+0x420).
class AtScriptHost {
public:
    AtScriptHost();                  // 0x000B4260
    void Init();                     // 0x000B42B0
    void Shutdown();                 // 0x000B42F0

    int m_running;                   // +0x00
    AtSmallCScriptFactory m_factory; // +0x04
    AtSmallCSystem m_system;         // +0x08

    static void SetHeap(void* heap);  // 0x000B4250
    static void* s_heap;             // 0x003ADE54
};

// The script layer's object, at +0x414 of App (vftable 0x0027D03C, three slots, 0x5B5C bytes). Unlike the others its running flag is a word of its own.
class AtScriptSystem {
public:
    AtScriptSystem();                                     // 0x0009EE60
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x0009EF50 not recovered
    virtual void Init(const AtScriptConfig* config, void* below, void* memory);  // slot 1, 0x0009EE80 below is the data management layer's object
    virtual void Shutdown();                              // slot 2, 0x0009EEC0
    void SetHeaps(const AtScriptConfig* config);          // 0x0009EE00 not virtual here

    int m_running;                   // +0x04 App+0x418
    void* m_below;                   // +0x08
    AtScriptHost m_host;             // +0x0C
};

// ---- the kinematics layer ---------------------------------------------------------------------------------------------

// What App's slot 31 fills in (0x34 bytes). Each heap that is 0 falls back to m_heap.
struct AtKinematicsConfig {
    int m_unknown00;                 // +0x00 not read
    AtHeap* m_heap;                  // +0x04 also what the factory is made from
    void* m_animBlendHeap;           // +0x08 AtSceneAnimBlend's
    void* m_animationHeap;           // +0x0C AtAnimationDatabase's
    void* m_coordSysHeap;            // +0x10 AtCoordSysDatabase's
    void* m_heap14;                  // +0x14 for 0x002C6348, by 0x000272D0
    void* m_factoryHeap;             // +0x18 AtGrD3DAnimationDevice's, for what it makes; by the derived class alone
    void* m_frameNumberHeap;         // +0x1C AtGrFrameNumberSet's
    void* m_heap20;                  // +0x20 for 0x003B23E8, by 0x001015B0
    void* m_heap24;                  // +0x24 for 0x003B23E4, by 0x00101390
    void* m_heap28;                  // +0x28 for 0x002C63DC, by 0x00034500
    void* m_heap2C;                  // +0x2C for 0x002C634C, by 0x00027850
    void* m_heap30;                  // +0x30 for 0x003B23E0, by 0x00100FB0
};
void AtSetHeap2C6348(void* heap);    // 0x000272D0
void AtSetHeap3B23E8(void* heap);    // 0x001015B0
void AtSetHeap3B23E4(void* heap);    // 0x00101390
void AtSetHeap2C63DC(void* heap);    // 0x00034500
void AtSetHeap2C634C(void* heap);    // 0x00027850
void AtSetHeap3B23E0(void* heap);    // 0x00100FB0
extern void* g_heap2C6348;           // 0x002C6348
extern void* g_heap3B23E8;           // 0x003B23E8
extern void* g_heap3B23E4;           // 0x003B23E4
extern void* g_heap2C63DC;           // 0x002C63DC
extern void* g_heap2C634C;           // 0x002C634C
extern void* g_heap3B23E0;           // 0x003B23E0

// What the kinematics layer makes on its heap (vftable 0x0028663C, 4 bytes) is the Xbox animation device, at/loader's AtGrD3DAnimationDevice
// (include/at/AtGrD3DDatabase.h): the same vftable, constructor 0x000C8920, destructor 0x000C8930 and heap 0x003ADF7C. at/atlayers called it
// AtGrD3DAnimationDevice; folded on integrate-batch7. Its slots 4 to 6 and SetHeap 0x000C8F50 are declared there.
class AtGrD3DAnimationDevice;

// vftable 0x0028662C (0x0C bytes).
class AtKinematicsSystemBase {
public:
    AtKinematicsSystemBase();                             // 0x000C8740
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000C87A0 not recovered
    virtual void Init(const AtKinematicsConfig* config, void* below, void* memory);  // slot 1, 0x000C8750
    virtual void Shutdown();                              // slot 2, 0x000C8780
    virtual void SetHeaps(const AtKinematicsConfig* config);  // slot 3, 0x000C8660

    AtGrD3DAnimationDevice* m_factory;  // +0x04 the derived class's; the base leaves it
    void* m_below;                   // +0x08 set while running
};

// The kinematics layer's object, at +0x5F70 of App (vftable 0x002847DC; App's constructor writes 0x0028DC6C over it, which differs in the destructors).
class AtKinematicsSystem : public AtKinematicsSystemBase {
public:
    AtKinematicsSystem();                                 // 0x000B34B0
    void DeletingDestructor(unsigned int flags) override; // slot 0, 0x000B3670 not recovered
    void Init(const AtKinematicsConfig* config, void* below, void* memory) override;  // slot 1, 0x000B34D0 below is the script layer's object
    void Shutdown() override;                             // slot 2, 0x000B35B0
    void SetHeaps(const AtKinematicsConfig* config) override;  // slot 3, 0x000B3580
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtMathConfig) == 0x10, "AtMathConfig");
static_assert(sizeof(AtMathSystem) == 0x0C, "AtMathSystem");
static_assert(sizeof(AtDataManagementConfig) == 0x34, "AtDataManagementConfig");
static_assert(offsetof(AtDataManagementSystem, m_below) == 0x80, "AtDataManagementSystem::m_below");
static_assert(sizeof(AtDataManagementSystem) == 0x84, "AtDataManagementSystem");
static_assert(sizeof(AtScriptConfig) == 0x14, "AtScriptConfig");
static_assert(sizeof(AtSmallCNativeEntry) == 0x1C, "AtSmallCNativeEntry");
static_assert(offsetof(AtSmallCSystem, m_scripts) == 0x3BC0, "AtSmallCSystem::m_scripts");
static_assert(offsetof(AtSmallCSystem, m_nativeCount) == 0x5B00, "AtSmallCSystem::m_nativeCount");
static_assert(offsetof(AtSmallCSystem, m_factory) == 0x5B14, "AtSmallCSystem::m_factory");
static_assert(offsetof(AtSmallCSystem, m_names) == 0x5B1C, "AtSmallCSystem::m_names");
static_assert(offsetof(AtSmallCSystem, m_running) == 0x5B44, "AtSmallCSystem::m_running");
static_assert(sizeof(AtSmallCSystem) == 0x5B48, "AtSmallCSystem");
static_assert(offsetof(AtScriptHost, m_system) == 0x08, "AtScriptHost::m_system");
static_assert(offsetof(AtScriptSystem, m_host) == 0x0C, "AtScriptSystem::m_host");
static_assert(sizeof(AtScriptSystem) == 0x5B5C, "AtScriptSystem");
static_assert(sizeof(AtKinematicsConfig) == 0x34, "AtKinematicsConfig");
static_assert(sizeof(AtKinematicsSystem) == 0x0C, "AtKinematicsSystem");
#pragma pop_macro("offsetof")
#endif
