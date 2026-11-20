// The behaviour scripts' abstract machine and what loads and runs them. Subsystem at/smallvm. Reference build knd-xbox-usa-5454008a-v1.
//
// The three compiled scripts on the disc (DESIGN\BEHAVIOR\*.ASL) are Small programs in the AMX format, and the code here is a Small abstract machine. The
// evidence, from this executable and those files only: the loader at 0x0007AB40 refuses a header whose word at +4 is not 0xF1E0 or whose word at +0x0A
// (the size of a table entry) is not 0x1C; the opcodes are numbered 1 to 133 in the order of Small's published instruction list (PROC 46, RETN 48,
// SYSREQ_C 123, LINE 125, CASETBL 130), and the scripts' code decodes cleanly with that list. Which version: the files say file version 3, required machine
// version 1; the header has 16-bit table counts each followed by a 32-bit offset (unaligned, at +0x20 to +0x3D); the machine has no hlw field and no
// opcodes past 133 (no NOP, SYSREQ_D, SYMTAG or BREAK), all of which places it before Small 2.x. It is not the published machine unchanged:
//   * A built-in is called as function(context, amx, params): each native entry holds a context word before the address (registered with the function).
//   * Opcodes 131 and 132, SWAP_PRI and SWAP_ALT in the published list, read and write host variables through a fifth table (+0x38/+0x3A).
//   * Opcode 133 (PUSHADDR in the published list) suspends the program: the state is kept in the script object and the next run resumes.
//   * LINE, while debugging, also suspends when the script system's step flag (+0x5B38) is set.
//   * Execution state the published machine keeps in locals partly lives in globals (0x00301B38, 0x00301B3C, 0x00301B40, 0x00301B44).
// Names of functions and members are inferred, after the published names where the role is the same (Init, Register, Exec, FindPublic, GetAddr,
// StrLen, GetString, RaiseError); no implementation was copied: everything here follows the machine code at the addresses beside it.
#pragma once
#include "at/AtApMidLayers.h"

// ---- the compiled program ---------------------------------------------------------------------------------------------

#pragma pack(push, 1)
// The first 0x40 bytes of a program, as 0x0007AB40 and 0x0007B1C0 read them. Offsets are from the start of the program.
struct AtAmxHeader {
    int m_size;                      // +0x00 bytes in the file
    unsigned short m_magic;          // +0x04 0xF1E0
    unsigned char m_fileVersion;     // +0x06 3 in every file; not read
    signed char m_amxVersion;        // +0x07 1 in every file; more than 3 is refused
    unsigned short m_flags;          // +0x08 bit 0 refused, bit 2 compact encoding
    unsigned short m_defsize;        // +0x0A 0x1C, the size of a table entry
    int m_cod;                       // +0x0C code
    int m_dat;                       // +0x10 data
    int m_hea;                       // +0x14 end of the data, start of the heap
    int m_stp;                       // +0x18 end of the memory: Init lowers it by one cell
    int m_cip;                       // +0x1C the code offset of main; negative for none
    short m_numPublics;              // +0x20
    int m_publics;                   // +0x22
    short m_numNatives;              // +0x26
    int m_natives;                   // +0x28
    short m_numLibraries;            // +0x2C not read
    int m_libraries;                 // +0x2E
    short m_numPubVars;              // +0x32 not read
    int m_pubVars;                   // +0x34
    short m_numHostVars;             // +0x38 the table opcodes 131 and 132 read (the published format's tags)
    int m_hostVars;                  // +0x3A
    unsigned short m_unknown3E;      // +0x3E
};
#pragma pack(pop)

// One entry of the publics, natives or host variables (0x1C bytes). For a native, Register fills the first two words; for a host variable, the second.
struct AtAmxFuncStub {
    unsigned int m_context;          // +0x00 a native's context (see AtAmxStoreHostRef)
    unsigned int m_address;          // +0x04 a public's code offset; a native's function; a host variable's address
    char m_name[0x14];               // +0x08
};

// ---- the machine ------------------------------------------------------------------------------------------------------

struct AtAmx;
typedef int (*AtAmxCallbackFn)(AtAmx* amx, int index, int* result, int* params);
typedef int (*AtAmxDebugFn)(AtAmx* amx);

// What the debug hook is told, in dbgcode.
enum { kAmxDbgInit = 0, kAmxDbgFile = 1, kAmxDbgLine = 2, kAmxDbgSymbol = 3, kAmxDbgClearSymbols = 4, kAmxDbgCall = 5, kAmxDbgReturn = 6,
       kAmxDbgTerminated = 7, kAmxDbgRange = 8 };
// Errors, as numbered by the code that returns them.
enum { kAmxErrNone = 0, kAmxErrStack = 3, kAmxErrBounds = 4, kAmxErrMemAccess = 5, kAmxErrInvalidInstruction = 6, kAmxErrStackLow = 7,
       kAmxErrHeapLow = 8, kAmxErrCallback = 9, kAmxErrDivide = 11, kAmxErrFormat = 17, kAmxErrVersion = 18, kAmxErrNotFound = 19, kAmxErrIndex = 20,
       kAmxErrDebug = 21, kAmxErrInit = 22 };
enum { kAmxFlagDebug = 2, kAmxFlagCompact = 4, kAmxFlagBrowse = 0x4000, kAmxFlagRelocated = 0x8000 };

// The machine's registers between calls (0x60 bytes). Addresses in hea, stk and stp are offsets from the start of the data.
struct AtAmx {
    unsigned char* m_base;           // +0x00 the program, header first
    AtAmxCallbackFn m_callback;      // +0x04 how SYSREQ reaches a native; Init sets AtAmxCallback when null
    AtAmxDebugFn m_debug;            // +0x08 Init sets AtAmxDebugNone when null
    int m_cip;                       // +0x0C stored before each native call
    int m_frm;                       // +0x10
    int m_hea;                       // +0x14
    int m_stk;                       // +0x18
    int m_stp;                       // +0x1C
    int m_flags;                     // +0x20
    int m_curline;                   // +0x24
    int m_curfile;                   // +0x28
    int m_dbgcode;                   // +0x2C
    int m_dbgaddr;                   // +0x30
    int m_dbgparam;                  // +0x34
    const char* m_dbgname;           // +0x38 into the code
    int m_user[8];                   // +0x3C not touched here
    int m_error;                     // +0x5C what a native raises
};

int AtAmxInit(AtAmx* amx, void* program);                                  // 0x0007AB40 0 even when relocation fails
int AtAmxRelocate(AtAmx* amx);                                             // 0x0007A890 amx in edi in the original
void AtAmxExpand(unsigned char* code, int size, int memsize);              // 0x0007AAE0 registers in the original: eax size, ecx memsize, edi code
int AtAmxCallback(AtAmx* amx, int index, int* result, int* params);        // 0x0007A7D0
int AtAmxHostVarGet(AtAmx* amx, int index, int* result, int* params);      // 0x0007A810 opcode 131; index is not read
int AtAmxHostVarSet(AtAmx* amx, int index, int* result, int* params);      // 0x0007A840 opcode 132; index is not read
int AtAmxDebugNone(AtAmx* amx);                                            // 0x0007A880 always 21, which leaves debugging off
int AtAmxFindPublic(AtAmx* amx, const char* name, int* index);             // 0x0007AC20 binary search; 0x7FFFFFFF and 19 when absent
AtSmallCNative AtAmxFindNativeFunction(const char* name, const AtSmallCNativeEntry* list, int count);  // 0x0007ACF0 count in edx; -1: to the empty name
void* AtAmxFindNativeContext(const char* name, const AtSmallCNativeEntry* list, int count);            // 0x0007AD60 likewise
int AtAmxRegister(AtAmx* amx, const AtSmallCNativeEntry* list, int count);  // 0x0007ADD0
int AtAmxRaiseError(AtAmx* amx, int error);                                // 0x0007AFC0
int AtAmxGetAddr(AtAmx* amx, int address, int** physical);                 // 0x0007AFD0
int AtAmxStrLen(const int* string, int* length);                           // 0x0007B000
int AtAmxGetString(char* dest, const int* source);                         // 0x0007B080
struct AtSmallCHostVar;
void* AtAmxFindHostVar(const char* name, const AtSmallCHostVar* list, int count);  // 0x0007B0F0 count in edx; -1 means no limit at all
int AtAmxRegisterHostVars(AtAmx* amx, const AtSmallCHostVar* list, int count);    // 0x0007B160
struct AtSmallCScript;
int AtAmxExec(AtSmallCScript* script, int* retval, int index, int numparams, const int* params);  // 0x0007B1C0 -1 when suspended

int AtCompareNames8(const char* a, const char* b);  // 0x00013F00 at most eight bytes as signed chars; null is empty

// What the machine keeps in globals (all written only by the code here).
struct AtAmxGlobals {
    static int s_littleEndian;       // 0x00301B34 set by Init; StrLen and ALIGN read it
    static int s_num;                // 0x00301B38 a native's result, SWITCH's count
    static int s_offs;               // 0x00301B3C the last operand
    static int s_op;                 // 0x00301B40 the last opcode
    static int s_debugging;          // 0x00301B44 the debug flag of the last start, kept across a resume
};

// A pointer stored in a 4-byte cell of the program (a native's function and context, a host variable's address). On a 32-bit build the pointer itself, as
// in the original; on a 64-bit build an index into a table kept for the process. The single place where the port differs from the original layout.
unsigned int AtAmxStoreHostRef(const void* pointer);
void* AtAmxLoadHostRef(unsigned int ref);

// ---- the host variables -----------------------------------------------------------------------------------------------

// One host variable a program may read and write by name (0x18 bytes), in the system's table at +0x3800.
struct AtSmallCHostVar {
    char m_name[0x14];               // +0x00
    int* m_address;                  // +0x14
};

// ---- the script object ------------------------------------------------------------------------------------------------

// What Load hands to StartScript (0x6C bytes, on the caller's stack).
struct AtSmallCScriptDesc {
    char m_file[8];                  // +0x00 the program's name, without .ASL
    char m_name[8];                  // +0x08 the script's name in the system
    int m_enabled;                   // +0x10 flag 4
    char m_entry[0x50];              // +0x14 the public to run; empty: main
    int m_flag40;                    // +0x64 flag 0x40; also handed to LoadProgram, which does not read it
    int m_shared;                    // +0x68 not 0: share the program of the script named m_file
};

// One script (0xAC bytes, "SmallCScript"): the machine, the program and what a suspended call needs to resume.
struct AtSmallCScript {
    enum { kFresh = 1, kInCall = 2, kEnabled = 4, kStarted = 8, kFinished = 0x10, kFlag40 = 0x40 };

    AtSmallCScript();                // 0x00075350 the owner and the machine are left alone
    ~AtSmallCScript();               // 0x000753F0 the same as Reset
    void Reset();                    // 0x00075390 frees the program unless shared
    void SetValid();                 // 0x00075340

    AtSmallCScriptFactory* m_owner;  // +0x00
    AtAmx m_amx;                     // +0x04
    char m_name[8];                  // +0x64 two words, not terminated
    unsigned char* m_program;        // +0x6C
    unsigned int m_flags;            // +0x70
    int m_valid;                     // +0x74
    int m_unknown78;                 // +0x78 always 4
    int m_entry;                     // +0x7C the public Run calls; -1 main
    int m_shared;                    // +0x80 the program belongs to another script
    // Kept by a suspension for the next run (0x0007B1C0); the pointers are the machine's own.
    AtAmxFuncStub* m_savedPublic;    // +0x84
    unsigned char* m_savedData;      // +0x88
    int m_savedPri;                  // +0x8C
    int m_savedAlt;                  // +0x90
    int m_savedStk;                  // +0x94
    int m_savedFrm;                  // +0x98
    int m_savedHea;                  // +0x9C
    int m_resetStk;                  // +0xA0 stored by every fresh start
    int* m_savedCip;                 // +0xA4
    int m_retval;                    // +0xA8
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtAmxHeader) == 0x40, "AtAmxHeader");
static_assert(offsetof(AtAmxHeader, m_hostVars) == 0x3A, "AtAmxHeader::m_hostVars");
static_assert(sizeof(AtAmxFuncStub) == 0x1C, "AtAmxFuncStub");
static_assert(offsetof(AtAmx, m_error) == 0x5C, "AtAmx::m_error");
static_assert(sizeof(AtAmx) == 0x60, "AtAmx");
static_assert(sizeof(AtSmallCHostVar) == 0x18, "AtSmallCHostVar");
static_assert(sizeof(AtSmallCScriptDesc) == 0x6C, "AtSmallCScriptDesc");
static_assert(offsetof(AtSmallCScript, m_program) == 0x6C, "AtSmallCScript::m_program");
static_assert(offsetof(AtSmallCScript, m_savedCip) == 0xA4, "AtSmallCScript::m_savedCip");
static_assert(sizeof(AtSmallCScript) == 0xAC, "AtSmallCScript");
#pragma pop_macro("offsetof")
#endif
