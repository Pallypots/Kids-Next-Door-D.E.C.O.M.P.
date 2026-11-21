// The behaviour scripts' abstract machine: the program's checks, compact decoding and relocation, natives and host variables, and the interpreter.
// Subsystem at/smallvm; see include/at/AtSmallVm.h. Original cluster 0x0007A7D0-0x0007C7F7 and 0x00013F00. Names are inferred.
#include "at/AtSmallVm.h"

#include "at/AtIoTokenizer.h"

int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp
void AtPushHeap(void* heap);
void AtPopHeap();
void AtRaise(int code, int file, int line, int unused);

int AtAmxGlobals::s_littleEndian;
int AtAmxGlobals::s_num;
int AtAmxGlobals::s_offs;
int AtAmxGlobals::s_op;
int AtAmxGlobals::s_debugging;

namespace {

// Cells may sit anywhere the program says; read and write them a byte at a time where the host might mind.
inline int Cell(const unsigned char* p) {
    int v;
    unsigned char* d = (unsigned char*)&v;
    d[0] = p[0];
    d[1] = p[1];
    d[2] = p[2];
    d[3] = p[3];
    return v;
}
inline void SetCell(unsigned char* p, int v) {
    const unsigned char* s = (const unsigned char*)&v;
    p[0] = s[0];
    p[1] = s[1];
    p[2] = s[2];
    p[3] = s[3];
}

int Compare(const char* a, const char* b) {
    for (;;) {
        const unsigned char x = (unsigned char)*a++;
        const unsigned char y = (unsigned char)*b++;
        if (x != y) {
            return x < y ? -1 : 1;
        }
        if (x == 0) {
            return 0;
        }
    }
}

unsigned int Length(const char* s) {
    const char* p = s;
    while (*p != 0) {
        p++;
    }
    return (unsigned int)(p - s);
}

AtAmxHeader* Header(const AtAmx* amx) { return (AtAmxHeader*)amx->m_base; }
AtAmxFuncStub* Stubs(const AtAmx* amx, int offset) { return (AtAmxFuncStub*)(amx->m_base + offset); }

// A relocated jump target. On a 32-bit build, as in the original, relocation adds the code's address to the operand, which then holds the target's address;
// a 64-bit build cannot fit the address in a cell and keeps the code offset, adding the code's address where the target is used.
#if defined(_M_IX86) || defined(__i386__)
inline void Relocate(unsigned char* operand, const unsigned char* code) { SetCell(operand, Cell(operand) + (int)(unsigned int)(unsigned long)code); }
inline const unsigned char* Target(int cell, const unsigned char*) { return (const unsigned char*)(unsigned long)(unsigned int)cell; }
#else
inline void Relocate(unsigned char*, const unsigned char*) {}
inline const unsigned char* Target(int cell, const unsigned char* code) { return code + cell; }
#endif

}  // namespace

// ---- pointers in cells --------------------------------------------------------------------------------------------------

#if defined(_M_IX86) || defined(__i386__)
unsigned int AtAmxStoreHostRef(const void* pointer) { return (unsigned int)(unsigned long)pointer; }
void* AtAmxLoadHostRef(unsigned int ref) { return (void*)(unsigned long)ref; }
#else
namespace {
const void* g_hostRefs[4096];
unsigned int g_hostRefCount;
}
unsigned int AtAmxStoreHostRef(const void* pointer) {
    if (pointer == 0) {
        return 0;
    }
    for (unsigned int i = 0; i < g_hostRefCount; i++) {
        if (g_hostRefs[i] == pointer) {
            return i + 1;
        }
    }
    if (g_hostRefCount >= sizeof(g_hostRefs) / sizeof(g_hostRefs[0])) {
        AtRaise(-4, 0, 0, 0);
        return 0;
    }
    g_hostRefs[g_hostRefCount++] = pointer;
    return g_hostRefCount;
}
void* AtAmxLoadHostRef(unsigned int ref) { return ref == 0 ? 0 : (void*)g_hostRefs[ref - 1]; }
#endif

// ---- natives and host variables ----------------------------------------------------------------------------------------

int AtAmxCallback(AtAmx* amx, int index, int* result, int* params) {
    AtAmxFuncStub* stub = Stubs(amx, Header(amx)->m_natives) + index;  // ORIGINAL: the index is not checked
    amx->m_error = 0;
    AtSmallCNative function = (AtSmallCNative)AtAmxLoadHostRef(stub->m_address);
    *result = function(AtAmxLoadHostRef(stub->m_context), amx, params);
    return amx->m_error;
}

int AtAmxHostVarGet(AtAmx* amx, int, int* result, int* params) {
    AtAmxFuncStub* stub = Stubs(amx, Header(amx)->m_hostVars) + params[1];  // ORIGINAL: the variable is the first argument, not the operand
    amx->m_error = 0;
    *result = *(int*)AtAmxLoadHostRef(stub->m_address);
    return amx->m_error;
}

int AtAmxHostVarSet(AtAmx* amx, int, int* result, int* params) {
    AtAmxFuncStub* stub = Stubs(amx, Header(amx)->m_hostVars) + params[1];
    amx->m_error = 0;
    *(int*)AtAmxLoadHostRef(stub->m_address) = params[2];
    *result = 0;
    return amx->m_error;
}

int AtAmxDebugNone(AtAmx*) {
    return kAmxErrDebug;
}

AtSmallCNative AtAmxFindNativeFunction(const char* name, const AtSmallCNativeEntry* list, int count) {
    for (int i = 0; list->m_name[0] != 0 && (i < count || count == -1); i++, list++) {
        if (Compare(name, list->m_name) == 0) {
            return list->m_function;
        }
    }
    return 0;
}

void* AtAmxFindNativeContext(const char* name, const AtSmallCNativeEntry* list, int count) {
    for (int i = 0; list->m_name[0] != 0 && (i < count || count == -1); i++, list++) {
        if (Compare(name, list->m_name) == 0) {
            return list->m_context;
        }
    }
    return 0;
}

int AtAmxRegister(AtAmx* amx, const AtSmallCNativeEntry* list, int count) {
    AtAmxHeader* header = Header(amx);
    AtAmxFuncStub* stub = Stubs(amx, header->m_natives);
    int error = kAmxErrNone;
    for (int i = 0; i < header->m_numNatives; i++, stub++) {
        if (stub->m_address != 0) {
            continue;
        }
        AtSmallCNative function = AtAmxFindNativeFunction(stub->m_name, list, count);
        if (function != 0) {
            void* context = AtAmxFindNativeContext(stub->m_name, list, count);
            if (context == 0) {
                error = kAmxErrNotFound;  // ORIGINAL: and registered all the same
            }
            stub->m_context = AtAmxStoreHostRef(context);
            stub->m_address = AtAmxStoreHostRef((const void*)function);
        } else {
            const char format[] = "\nWARNING: Amx function %s not found.\n";
            const unsigned int size = Length(format);
            if (AtIoTokenFile::s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(AtIoTokenFile::s_heap);
            // ORIGINAL: the block is the format's length, and the message goes past the name's length plus one into it; that pointer is what is tested,
            // written and freed. Nothing prints it.
            char* message = (char*)operator new(size) + Length(stub->m_name) + 1;
            AtPopHeap();
            if (message == 0) {
                AtRaise(-4, 0, 0, 0);
            } else {
                AtFormat(message, format, stub->m_name);
                operator delete(message);
            }
            error = kAmxErrNotFound;
        }
    }
    return error;
}

void* AtAmxFindHostVar(const char* name, const AtSmallCHostVar* list, int count) {
    if (list == 0) {
        return 0;
    }
    // ORIGINAL: with count -1 nothing ends the search but a match.
    for (int i = 0; i < count || count == -1; i++) {
        if (Compare(name, list[i].m_name) == 0) {
            return list[i].m_address;
        }
    }
    return 0;
}

int AtAmxRegisterHostVars(AtAmx* amx, const AtSmallCHostVar* list, int count) {
    AtAmxHeader* header = Header(amx);
    AtAmxFuncStub* stub = Stubs(amx, header->m_hostVars);
    int error = kAmxErrNone;
    for (int i = 0; i < header->m_numHostVars; i++, stub++) {
        void* address = AtAmxFindHostVar(stub->m_name, list, count);
        if (address != 0) {
            stub->m_address = AtAmxStoreHostRef(address);
        } else {
            error = kAmxErrNotFound;
        }
    }
    return error;
}

int AtAmxFindPublic(AtAmx* amx, const char* name, int* index) {
    AtAmxHeader* header = Header(amx);
    char entry[0x14];
    int low = 0;
    int high = header->m_numPublics - 1;
    while (low <= high) {
        const int middle = (low + high) / 2;
        if (middle < header->m_numPublics) {
            const char* from = Stubs(amx, header->m_publics)[middle].m_name;
            char* to = entry;
            while ((*to++ = *from++) != 0) {
            }
        }
        const int order = Compare(entry, name);
        if (order > 0) {
            high = middle - 1;
        } else if (order < 0) {
            low = middle + 1;
        } else {
            *index = middle;
            return kAmxErrNone;
        }
    }
    *index = 0x7FFFFFFF;
    return kAmxErrNotFound;
}

int AtAmxRaiseError(AtAmx* amx, int error) {
    amx->m_error = error;
    return kAmxErrNone;
}

int AtAmxGetAddr(AtAmx* amx, int address, int** physical) {
    if ((address >= amx->m_hea && address < amx->m_stk) || address >= amx->m_stp) {  // ORIGINAL: a negative address passes
        return kAmxErrMemAccess;
    }
    *physical = (int*)(amx->m_base + Header(amx)->m_dat + address);
    return kAmxErrNone;
}

int AtAmxStrLen(const int* string, int* length) {
    int count;
    if (string[0] > 0xFF) {
        count = (int)Length((const char*)string);
        if (AtAmxGlobals::s_littleEndian != 0) {
            unsigned int c = (unsigned int)string[(unsigned int)count / 4];
            count -= (unsigned int)count % 4;
            while ((c & 0xFF000000u) != 0) {
                count++;
                c <<= 8;
            }
        }
    } else {
        count = 0;
        if (string[0] != 0) {
            do {
                count++;
            } while (string[count] != 0);
        }
    }
    *length = count;
    return kAmxErrNone;
}

int AtAmxGetString(char* dest, const int* source) {
    if (source[0] > 0xFF) {
        int cell = 0;
        int byte = 3;
        char c;
        do {
            if (byte == 3) {
                cell = *source++;
            }
            c = (char)(cell >> (byte * 8));
            byte = (byte - 1) & 3;
            *dest++ = c;
        } while (c != 0);
        return kAmxErrNone;
    }
    while (*source != 0) {
        *dest++ = (char)*source++;
    }
    *dest = 0;
    return kAmxErrNone;
}

// AtCompareNames8 (0x00013F00) is at/fatal's AtCompareName8, src/at/AtGr2D.cpp: folded on integrate-batch8.

// ---- loading ------------------------------------------------------------------------------------------------------------

void AtAmxExpand(unsigned char* code, int size, int memsize) {
    unsigned char* out = code + memsize;
    while (size > 0) {
        unsigned int c = 0;
        int shift = 0;
        do {
            size--;
            c |= (unsigned int)(code[size] & 0x7F) << (shift & 31);  // ORIGINAL: past four bytes the shift wraps, as x86 shifts do
            shift += 7;
        } while (size > 0 && (code[size - 1] & 0x80) != 0);
        if ((code[size] & 0x40) != 0) {
            while (shift < 32) {
                c |= 0xFFu << shift;
                shift += 8;
            }
        }
        out -= 4;
        SetCell(out, (int)c);
    }
}

int AtAmxRelocate(AtAmx* amx) {
    AtAmxHeader* header = Header(amx);
    unsigned char* code = amx->m_base + header->m_cod;
    const int size = header->m_dat - header->m_cod;
    int symbolTest = 0;  // ORIGINAL: set by each SYMBOL, read by SYMBOL and SRANGE alike
    amx->m_dbgcode = kAmxDbgInit;
    amx->m_flags = kAmxFlagBrowse;
    const int debugging = amx->m_debug(amx) == 0;
    if (debugging) {
        amx->m_flags = kAmxFlagBrowse | kAmxFlagDebug;
    }
    for (int cip = 0; cip < size;) {
        const int op = Cell(code + cip);
        if (op >= 0x100 || (unsigned int)(op - 1) > 0x84) {
            return kAmxErrInvalidInstruction;
        }
        cip += 4;
        switch (op) {
        case 49: case 51: case 53: case 54: case 55: case 56: case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64: case 129:
            Relocate(code + cip, code);  // CALL, JUMP, the conditional jumps, SWITCH
            cip += 4;
            break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18:
        case 19: case 20: case 21: case 22: case 24: case 26: case 28: case 29: case 30: case 31: case 32: case 38: case 39: case 40: case 41: case 44:
        case 45: case 52: case 68: case 69: case 70: case 71: case 87: case 88: case 91: case 92: case 105: case 106: case 109: case 110: case 114:
        case 115: case 117: case 118: case 119: case 120: case 121: case 123: case 131: case 132:
            cip += 4;
            break;
        case 124: {  // FILE
            const int length = Cell(code + cip);
            const int file = Cell(code + cip + 4);
            cip += 8;
            amx->m_curfile = file;
            amx->m_dbgcode = kAmxDbgFile;
            amx->m_dbgname = (const char*)(code + cip);
            cip += length - 4;
            if (debugging) {
                amx->m_debug(amx);
            }
            break;
        }
        case 125:  // LINE
            amx->m_dbgcode = kAmxDbgLine;
            amx->m_curline = Cell(code + cip);
            cip += 4;
            amx->m_curfile = Cell(code + cip);
            cip += 4;
            if (debugging) {
                amx->m_debug(amx);
            }
            break;
        case 126: {  // SYMBOL
            const int length = Cell(code + cip);
            amx->m_dbgaddr = Cell(code + cip + 4);
            cip += 8;
            amx->m_dbgparam = Cell(code + cip);
            symbolTest = (amx->m_dbgparam & 0xFFFFFF00) == 0;
            cip += 4;
            amx->m_dbgcode = kAmxDbgSymbol;
            amx->m_dbgname = (const char*)(code + cip);
            cip += length - 8;
            if (debugging && symbolTest) {
                amx->m_debug(amx);
            }
            break;
        }
        case 127:  // SRANGE
            amx->m_dbgaddr = Cell(code + cip);
            cip += 4;
            amx->m_dbgparam = Cell(code + cip);
            cip += 4;
            amx->m_dbgcode = kAmxDbgRange;
            if (debugging && symbolTest) {
                amx->m_debug(amx);
            }
            break;
        case 130: {  // CASETBL: the default and each case's target
            const int cases = Cell(code + cip);
            cip += 4;
            if (cases >= 0) {
                for (int i = 0; i <= cases; i++) {
                    Relocate(code + cip + i * 8, code);
                }
            }
            cip += cases * 8 + 4;
            break;
        }
        default:
            break;
        }
    }
    amx->m_flags |= kAmxFlagRelocated;
    return kAmxErrNone;
}

int AtAmxInit(AtAmx* amx, void* program) {
    AtAmxHeader* header = (AtAmxHeader*)program;
    if (header->m_magic != 0xF1E0 || header->m_defsize != 0x1C) {
        return kAmxErrFormat;
    }
    if (header->m_amxVersion > 3) {
        return kAmxErrVersion;
    }
    if (header->m_stp >= 0x7FFFFFFF || header->m_stp <= 0) {
        return kAmxErrFormat;
    }
    if ((header->m_flags & 1) != 0) {
        return kAmxErrFormat;
    }
    if ((header->m_flags & kAmxFlagCompact) != 0) {
        AtAmxExpand((unsigned char*)program + header->m_cod, header->m_size - header->m_cod, header->m_hea - header->m_cod);
    }
    amx->m_base = (unsigned char*)program;
    header->m_stp -= 4;  // ORIGINAL: in the program's own header
    SetCell(amx->m_base + header->m_stp, 0);
    amx->m_hea = header->m_hea - header->m_dat;
    amx->m_stk = amx->m_stp = header->m_stp - header->m_dat;
    if (amx->m_callback == 0) {
        amx->m_callback = AtAmxCallback;
    }
    if (amx->m_debug == 0) {
        amx->m_debug = AtAmxDebugNone;
    }
    amx->m_curline = 0;
    amx->m_curfile = 0;
    AtAmxGlobals::s_littleEndian = 1;
    AtAmxRelocate(amx);  // ORIGINAL: its result is not looked at; Exec refuses what it did not finish
    return kAmxErrNone;
}

// ---- the interpreter ----------------------------------------------------------------------------------------------------

int AtAmxExec(AtSmallCScript* script, int* retval, int index, int numparams, const int* params) {
    AtAmx* amx = &script->m_amx;
    AtAmxHeader* header;
    const unsigned char* code;
    unsigned char* data;
    const int* cip;
    AtAmxFuncStub* publicStub = 0;
    int pri, alt, stk, frm, hea, resetStk;
    int& offs = AtAmxGlobals::s_offs;
    int& num = AtAmxGlobals::s_num;

    if ((script->m_flags & AtSmallCScript::kFresh) != 0) {
        if (amx->m_callback == 0) {
            return kAmxErrCallback;
        }
        amx->m_flags &= ~kAmxFlagBrowse;
        if ((amx->m_flags & kAmxFlagRelocated) == 0) {
            return kAmxErrInit;
        }
        AtAmxGlobals::s_debugging = (amx->m_flags >> 1) & 1;
        header = Header(amx);
        code = amx->m_base + header->m_cod;
        data = amx->m_base + header->m_dat;
        stk = amx->m_stk;
        hea = amx->m_hea;
        script->m_resetStk = stk;
        resetStk = stk;
        if (stk > header->m_stp) {  // ORIGINAL: against the header's stp, an offset from the program, not from the data
            return kAmxErrStackLow;
        }
        if (hea < 0) {
            return kAmxErrHeapLow;
        }
        if (index == -1) {
            if (header->m_cip < 0) {
                return kAmxErrIndex;
            }
            cip = (const int*)(code + header->m_cip);
        } else {
            if ((index < 0 && index != -2) || index >= header->m_numPublics) {
                return kAmxErrIndex;
            }
            publicStub = Stubs(amx, header->m_publics) + index;  // ORIGINAL: -2 reads the entry two before the first
            cip = (const int*)(code + publicStub->m_address);
        }
        if (AtAmxGlobals::s_debugging != 0) {
            amx->m_dbgcode = kAmxDbgCall;
            amx->m_dbgaddr = (int)((const unsigned char*)cip - code);
            amx->m_debug(amx);
        }
        const int bytes = numparams * 4;
        stk -= bytes;
        for (int i = 0; i < numparams; i++) {
            SetCell(data + stk + i * 4, params[i]);
        }
        stk -= 4;
        SetCell(data + stk, bytes);
        stk -= 4;
        SetCell(data + stk, 0);
        if (hea + 0x40 > stk) {
            return kAmxErrStack;
        }
        pri = 0;
        alt = 0;
        frm = 0;
    } else {
        header = Header(amx);
        code = amx->m_base + header->m_cod;
        resetStk = script->m_resetStk;
        publicStub = script->m_savedPublic;
        data = script->m_savedData;
        pri = script->m_savedPri;
        alt = script->m_savedAlt;
        stk = script->m_savedStk;
        frm = script->m_savedFrm;
        hea = script->m_savedHea;
        cip = script->m_savedCip;
    }

#define CELL(a) Cell(data + (a))
#define SET(a, v) SetCell(data + (a), (v))
#define OPERAND() (*cip++)
#define FAIL(e)                  \
    do {                         \
        amx->m_stk = resetStk;   \
        return (e);              \
    } while (0)
#define OUT_OF_BOUNDS(a) (((a) >= hea && (a) < stk) || (a) >= amx->m_stp)  // ORIGINAL: a negative address passes

    for (;;) {
        const int op = *cip++;
        AtAmxGlobals::s_op = op;
        switch (op) {
        case 1: offs = OPERAND(); pri = CELL(offs); break;                     // LOAD_PRI
        case 2: offs = OPERAND(); alt = CELL(offs); break;                     // LOAD_ALT
        case 3: offs = OPERAND(); pri = CELL(offs + frm); break;               // LOAD_S_PRI
        case 4: offs = OPERAND(); alt = CELL(offs + frm); break;               // LOAD_S_ALT
        case 5: offs = CELL(OPERAND()); pri = CELL(offs); break;               // LREF_PRI
        case 6: offs = CELL(OPERAND()); alt = CELL(offs); break;               // LREF_ALT
        case 7: offs = CELL(OPERAND() + frm); pri = CELL(offs); break;         // LREF_S_PRI
        case 8: offs = CELL(OPERAND() + frm); alt = CELL(offs); break;         // LREF_S_ALT
        case 9:                                                                // LOAD_I
            if (OUT_OF_BOUNDS(pri)) {
                FAIL(kAmxErrMemAccess);
            }
            pri = CELL(pri);
            break;
        case 10:  // LODB_I
            offs = OPERAND();
            if (OUT_OF_BOUNDS(pri)) {
                FAIL(kAmxErrMemAccess);
            }
            if (offs == 1) {
                pri = data[pri];
            } else if (offs == 2) {
                pri = data[pri] | (data[pri + 1] << 8);
            } else if (offs == 4) {
                pri = CELL(pri);
            }
            break;
        case 11: pri = OPERAND(); break;                                       // CONST_PRI
        case 12: alt = OPERAND(); break;                                       // CONST_ALT
        case 13: pri = OPERAND() + frm; break;                                 // ADDR_PRI
        case 14: alt = OPERAND() + frm; break;                                 // ADDR_ALT
        case 15: offs = OPERAND(); SET(offs, pri); break;                      // STOR_PRI
        case 16: offs = OPERAND(); SET(offs, alt); break;                      // STOR_ALT
        case 17: offs = OPERAND(); SET(offs + frm, pri); break;                // STOR_S_PRI
        case 18: offs = OPERAND(); SET(offs + frm, alt); break;                // STOR_S_ALT
        case 19: offs = CELL(OPERAND()); SET(offs, pri); break;                // SREF_PRI
        case 20: offs = CELL(OPERAND()); SET(offs, alt); break;                // SREF_ALT
        case 21: offs = CELL(OPERAND() + frm); SET(offs, pri); break;          // SREF_S_PRI
        case 22: offs = CELL(OPERAND() + frm); SET(offs, alt); break;          // SREF_S_ALT
        case 23:                                                               // STOR_I
            if (OUT_OF_BOUNDS(alt)) {
                FAIL(kAmxErrMemAccess);
            }
            SET(alt, pri);
            break;
        case 24:  // STRB_I
            offs = OPERAND();
            if (OUT_OF_BOUNDS(alt)) {
                FAIL(kAmxErrMemAccess);
            }
            if (offs == 1) {
                data[alt] = (unsigned char)pri;
            } else if (offs == 2) {
                data[alt] = (unsigned char)pri;
                data[alt + 1] = (unsigned char)(pri >> 8);
            } else if (offs == 4) {
                SET(alt, pri);
            }
            break;
        case 25:  // LIDX
            offs = (int)((unsigned int)alt + (unsigned int)pri * 4);
            if (OUT_OF_BOUNDS(offs)) {
                FAIL(kAmxErrMemAccess);
            }
            pri = CELL(offs);
            break;
        case 26: {  // LIDX_B
            const int shift = OPERAND();
            offs = (int)((unsigned int)pri << (shift & 31)) + alt;
            if (OUT_OF_BOUNDS(offs)) {
                FAIL(kAmxErrMemAccess);
            }
            pri = CELL(offs);
            break;
        }
        case 27: pri = (int)((unsigned int)alt + (unsigned int)pri * 4); break;                                   // IDXADDR
        case 28: offs = OPERAND(); pri = (int)((unsigned int)pri << (offs & 31)) + alt; break;  // IDXADDR_B
        case 29:                                                               // ALIGN_PRI
            offs = OPERAND();
            if (AtAmxGlobals::s_littleEndian != 0 && (unsigned int)offs < 4) {
                pri ^= 4 - offs;
            }
            break;
        case 30:                                                               // ALIGN_ALT
            offs = OPERAND();
            if (AtAmxGlobals::s_littleEndian != 0 && (unsigned int)offs < 4) {
                alt ^= 4 - offs;
            }
            break;
        case 31:  // LCTRL
            offs = OPERAND();
            switch (offs) {
            case 0: pri = header->m_cod; break;
            case 1: pri = header->m_dat; break;
            case 2: pri = hea; break;
            case 3: pri = amx->m_stp; break;
            case 4: pri = stk; break;
            case 5: pri = frm; break;
            case 6: pri = (int)((const unsigned char*)cip - code); break;
            default: break;
            }
            break;
        case 32:  // SCTRL
            offs = OPERAND();
            switch (offs) {
            case 2: hea = pri; break;
            case 4: stk = pri; break;
            case 5: frm = pri; break;
            case 6: cip = (const int*)(code + pri); break;
            default: break;  // ORIGINAL: 3 (stp) is ignored too
            }
            break;
        case 33: pri = alt; break;                                             // MOVE_PRI
        case 34: alt = pri; break;                                             // MOVE_ALT
        case 35: offs = pri; pri = alt; alt = offs; break;                     // XCHG
        case 36: stk -= 4; SET(stk, pri); break;                               // PUSH_PRI
        case 37: stk -= 4; SET(stk, alt); break;                               // PUSH_ALT
        case 38:                                                               // PUSH_R
            offs = OPERAND();
            for (unsigned int n = (unsigned int)offs; n != 0; n--) {
                stk -= 4;
                SET(stk, pri);
            }
            offs = -1;
            break;
        case 39: offs = OPERAND(); stk -= 4; SET(stk, offs); break;            // PUSH_C
        case 40: offs = OPERAND(); stk -= 4; SET(stk, CELL(offs)); break;      // PUSH
        case 41: offs = OPERAND(); stk -= 4; SET(stk, CELL(offs + frm)); break;  // PUSH_S
        case 42: pri = CELL(stk); stk += 4; break;                             // POP_PRI
        case 43: alt = CELL(stk); stk += 4; break;                             // POP_ALT
        case 44:  // STACK
            alt = stk;
            offs = OPERAND();
            stk += offs;
            if (hea + 0x40 > stk) {
                return kAmxErrStack;  // ORIGINAL: this and the next leave the machine's stk as it was
            }
            if (stk > header->m_stp) {
                return kAmxErrStackLow;
            }
            if (AtAmxGlobals::s_debugging != 0 && offs > 0) {
                amx->m_dbgcode = kAmxDbgClearSymbols;
                amx->m_stk = stk;
                amx->m_debug(amx);
            }
            break;
        case 45:  // HEAP
            alt = hea;
            offs = OPERAND();
            hea += offs;
            if (hea + 0x40 > stk) {
                return kAmxErrStack;
            }
            if (hea < 0) {
                return kAmxErrHeapLow;
            }
            break;
        case 46: stk -= 4; SET(stk, frm); frm = stk; break;                    // PROC
        case 47:  // RET
            frm = CELL(stk);
            stk += 4;
            offs = CELL(stk);
            stk += 4;
            cip = (const int*)(code + offs);
            if (offs == 0) {
                if (retval != 0) {
                    *retval = pri;
                }
                if (AtAmxGlobals::s_debugging != 0) {
                    amx->m_dbgcode = kAmxDbgTerminated;
                    amx->m_debug(amx);
                }
                amx->m_stk = resetStk;
                return kAmxErrNone;
            }
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_dbgcode = kAmxDbgReturn;
                amx->m_debug(amx);
            }
            break;
        case 48:  // RETN
            frm = CELL(stk);
            stk += 4;
            offs = CELL(stk);
            stk += 4;
            cip = (const int*)(code + offs);
            stk += CELL(stk) + 4;
            amx->m_stk = stk;
            if (offs == 0) {
                if (retval != 0) {
                    *retval = pri;
                }
                if (AtAmxGlobals::s_debugging != 0) {
                    amx->m_dbgcode = kAmxDbgTerminated;
                    amx->m_debug(amx);
                }
                return kAmxErrNone;  // ORIGINAL: unlike RET, stk is left where the return put it
            }
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_dbgcode = kAmxDbgReturn;
                amx->m_debug(amx);
                if (AtAmxGlobals::s_debugging != 0) {
                    amx->m_dbgcode = kAmxDbgClearSymbols;
                    amx->m_debug(amx);
                }
            }
            break;
        case 49: {  // CALL
            stk -= 4;
            SET(stk, (int)((const unsigned char*)cip - code) + 4);
            cip = (const int*)Target(*cip, code);
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_dbgcode = kAmxDbgCall;
                amx->m_dbgaddr = (int)((const unsigned char*)cip - code);
                amx->m_debug(amx);
            }
            break;
        }
        case 50:  // CALL_PRI
            stk -= 4;
            SET(stk, (int)((const unsigned char*)cip - code));
            cip = (const int*)(code + pri);
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_dbgcode = kAmxDbgCall;
                amx->m_dbgaddr = pri;
                amx->m_debug(amx);
            }
            break;
        case 52:   // JREL
        case 124:  // FILE, skipped the same way
            offs = *cip;
            cip = (const int*)((const unsigned char*)cip + offs + 4);
            break;
        case 51: cip = (const int*)Target(*cip, code); break;                                        // JUMP
        case 53: if (pri == 0) cip = (const int*)Target(*cip, code); else cip++; break;              // JZER
        case 54: if (pri != 0) cip = (const int*)Target(*cip, code); else cip++; break;              // JNZ
        case 55: if (pri == alt) cip = (const int*)Target(*cip, code); else cip++; break;            // JEQ
        case 56: if (pri != alt) cip = (const int*)Target(*cip, code); else cip++; break;            // JNEQ
        case 57: if ((unsigned int)pri < (unsigned int)alt) cip = (const int*)Target(*cip, code); else cip++; break;   // JLESS
        case 58: if ((unsigned int)pri <= (unsigned int)alt) cip = (const int*)Target(*cip, code); else cip++; break;  // JLEQ
        case 59: if ((unsigned int)pri > (unsigned int)alt) cip = (const int*)Target(*cip, code); else cip++; break;   // JGRTR
        case 60: if ((unsigned int)pri >= (unsigned int)alt) cip = (const int*)Target(*cip, code); else cip++; break;  // JGEQ
        case 61: if (pri < alt) cip = (const int*)Target(*cip, code); else cip++; break;             // JSLESS
        case 62: if (pri <= alt) cip = (const int*)Target(*cip, code); else cip++; break;            // JSLEQ
        case 63: if (pri > alt) cip = (const int*)Target(*cip, code); else cip++; break;             // JSGRTR
        case 64: if (pri >= alt) cip = (const int*)Target(*cip, code); else cip++; break;            // JSGEQ
        case 65: pri = (int)((unsigned int)pri << (alt & 31)); break;          // SHL
        case 66: pri = (int)((unsigned int)pri >> (alt & 31)); break;          // SHR
        case 67: pri >>= alt & 31; break;                                      // SSHR
        case 68: offs = OPERAND(); pri = (int)((unsigned int)pri << (offs & 31)); break;  // SHL_C_PRI
        case 69: offs = OPERAND(); alt = (int)((unsigned int)alt << (offs & 31)); break;  // SHL_C_ALT
        case 70: offs = OPERAND(); pri = (int)((unsigned int)pri >> (offs & 31)); break;  // SHR_C_PRI
        case 71: offs = OPERAND(); alt = (int)((unsigned int)alt >> (offs & 31)); break;  // SHR_C_ALT
        case 72:                                                               // SMUL
        case 75:                                                               // UMUL
            pri = (int)((unsigned int)pri * (unsigned int)alt);
            break;
        case 73:  // SDIV: rounds down, the remainder takes the divisor's sign
            if (alt == 0) {
                FAIL(kAmxErrDivide);
            }
            offs = (int)((unsigned int)(pri % alt) + (unsigned int)alt) % alt;
            pri = (int)((unsigned int)pri - (unsigned int)offs) / alt;
            alt = offs;
            break;
        case 74:  // SDIV_ALT
            if (pri == 0) {
                FAIL(kAmxErrDivide);
            }
            offs = (int)((unsigned int)(alt % pri) + (unsigned int)pri) % pri;
            pri = (int)((unsigned int)alt - (unsigned int)offs) / pri;
            alt = offs;
            break;
        case 76:  // UDIV
            if (alt == 0) {
                FAIL(kAmxErrDivide);
            }
            offs = (int)((unsigned int)pri % (unsigned int)alt);
            pri = (int)((unsigned int)pri / (unsigned int)alt);
            alt = offs;
            break;
        case 77:  // UDIV_ALT
            if (pri == 0) {
                FAIL(kAmxErrDivide);
            }
            offs = (int)((unsigned int)alt % (unsigned int)pri);
            pri = (int)((unsigned int)alt / (unsigned int)pri);
            alt = offs;
            break;
        case 78: pri = (int)((unsigned int)pri + (unsigned int)alt); break;    // ADD
        case 79: pri = (int)((unsigned int)pri - (unsigned int)alt); break;    // SUB
        case 80: pri = (int)((unsigned int)alt - (unsigned int)pri); break;    // SUB_ALT
        case 81: pri &= alt; break;                                            // AND
        case 82: pri |= alt; break;                                            // OR
        case 83: pri ^= alt; break;                                            // XOR
        case 84: pri = pri == 0; break;                                        // NOT
        case 85: pri = (int)(0u - (unsigned int)pri); break;                   // NEG
        case 86: pri = ~pri; break;                                            // INVERT
        case 87: offs = OPERAND(); pri = (int)((unsigned int)pri + (unsigned int)offs); break;  // ADD_C
        case 88: offs = OPERAND(); pri = (int)((unsigned int)offs * (unsigned int)pri); break;  // SMUL_C
        case 89: pri = 0; break;                                               // ZERO_PRI
        case 90: alt = 0; break;                                               // ZERO_ALT
        case 91: offs = OPERAND(); SET(offs, 0); break;                        // ZERO
        case 92: offs = OPERAND(); SET(offs + frm, 0); break;                  // ZERO_S
        case 93: if ((pri & 0xFF) >= 0x80) pri |= (int)0xFFFFFF00; break;      // SIGN_PRI
        case 94: if ((alt & 0xFF) >= 0x80) alt |= (int)0xFFFFFF00; break;      // SIGN_ALT
        case 95: pri = pri == alt; break;                                      // EQ
        case 96: pri = pri != alt; break;                                      // NEQ
        case 97: pri = (unsigned int)pri < (unsigned int)alt; break;           // LESS
        case 98: pri = (unsigned int)pri <= (unsigned int)alt; break;          // LEQ
        case 99: pri = (unsigned int)pri > (unsigned int)alt; break;           // GRTR
        case 100: pri = (unsigned int)pri >= (unsigned int)alt; break;         // GEQ
        case 101: pri = pri < alt; break;                                      // SLESS
        case 102: pri = pri <= alt; break;                                     // SLEQ
        case 103: pri = pri > alt; break;                                      // SGRTR
        case 104: pri = pri >= alt; break;                                     // SGEQ
        case 105: offs = OPERAND(); pri = pri == offs; break;                  // EQ_C_PRI
        case 106: offs = OPERAND(); pri = alt == offs; break;                  // EQ_C_ALT
        case 107: pri++; break;                                                // INC_PRI
        case 108: alt++; break;                                                // INC_ALT
        case 109: offs = OPERAND(); SET(offs, CELL(offs) + 1); break;          // INC
        case 110: offs = OPERAND(); SET(offs + frm, CELL(offs + frm) + 1); break;  // INC_S
        case 111: SET(pri, CELL(pri) + 1); break;                              // INC_I
        case 112: pri--; break;                                                // DEC_PRI
        case 113: alt--; break;                                                // DEC_ALT
        case 114: offs = OPERAND(); SET(offs, CELL(offs) - 1); break;          // DEC
        case 115: offs = OPERAND(); SET(offs + frm, CELL(offs + frm) - 1); break;  // DEC_S
        case 116: SET(pri, CELL(pri) - 1); break;                              // DEC_I
        case 117: {  // MOVS: words forward, then the bytes left over; no bounds
            offs = OPERAND();
            const unsigned char* from = data + pri;
            unsigned char* to = data + alt;
            for (unsigned int n = (unsigned int)offs >> 2; n != 0; n--) {
                SetCell(to, Cell(from));
                to += 4;
                from += 4;
            }
            for (unsigned int n = (unsigned int)offs & 3; n != 0; n--) {
                *to++ = *from++;
            }
            break;
        }
        case 118: {  // CMPS: alt's bytes against pri's
            offs = OPERAND();
            const unsigned char* a = data + alt;
            const unsigned char* b = data + pri;
            pri = 0;
            for (unsigned int n = (unsigned int)offs; n != 0; n--, a++, b++) {
                if (*a != *b) {
                    pri = *a < *b ? -1 : 1;
                    break;
                }
            }
            break;
        }
        case 119: {  // FILL
            offs = OPERAND();
            if ((unsigned int)offs < 4) {
                break;
            }
            unsigned int n = (unsigned int)offs >> 2;
            unsigned char* to = data + alt;
            offs = (int)((unsigned int)offs - n * 4);
            for (; n != 0; n--) {
                SetCell(to, pri);
                to += 4;
            }
            break;
        }
        case 120:  // HALT
            offs = *cip;
            if (retval != 0) {
                *retval = pri;
            }
            amx->m_stk = resetStk;
            return offs;
        case 121:  // BOUNDS
            offs = OPERAND();
            if (pri > offs || pri < 0) {
                FAIL(kAmxErrBounds);
            }
            break;
        case 122:  // SYSREQ_PRI
        case 123:  // SYSREQ_C
        case 131:  // a host variable read
        case 132:  // a host variable written
        {
            int which = pri;
            if (op != 122) {
                which = *cip++;
                offs = which;
            }
            amx->m_cip = (int)((const unsigned char*)cip - code);
            amx->m_frm = frm;
            amx->m_stk = stk;
            amx->m_hea = hea;
            if (op == 131) {
                num = AtAmxHostVarGet(amx, which, &pri, (int*)(data + stk));
            } else if (op == 132) {
                num = AtAmxHostVarSet(amx, which, &pri, (int*)(data + stk));
            } else {
                num = amx->m_callback(amx, which, &pri, (int*)(data + stk));
            }
            if (num != 0) {
                FAIL(num);
            }
            break;
        }
        case 125:  // LINE
            amx->m_curline = *cip++;
            amx->m_curfile = *cip++;
            if (AtAmxGlobals::s_debugging == 0) {
                break;
            }
            amx->m_frm = frm;
            amx->m_stk = stk;
            amx->m_dbgcode = kAmxDbgLine;
            amx->m_debug(amx);
            if (AtSmallCSystem::Instance()->m_stepMode == 0) {
                break;
            }
            // fall through: the step suspends
        case 133:  // suspend: the next run resumes after this instruction
            script->m_savedPublic = publicStub;
            script->m_savedData = data;
            script->m_savedAlt = alt;
            script->m_savedStk = stk;
            script->m_savedCip = (int*)cip;
            script->m_savedPri = pri;
            script->m_savedFrm = frm;
            script->m_savedHea = hea;
            return -1;
        case 126: {  // SYMBOL
            const int length = *cip++;
            amx->m_dbgaddr = *cip++;
            amx->m_dbgparam = *cip++;
            amx->m_dbgname = (const char*)cip;
            offs = length;
            cip = (const int*)((const unsigned char*)cip + length - 8);
            amx->m_dbgcode = kAmxDbgSymbol;
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_frm = frm;
                amx->m_debug(amx);
            }
            break;
        }
        case 127:  // SRANGE
            amx->m_dbgaddr = *cip++;
            amx->m_dbgparam = *cip++;
            amx->m_dbgcode = kAmxDbgRange;
            if (AtAmxGlobals::s_debugging != 0) {
                amx->m_frm = frm;
                amx->m_debug(amx);
            }
            break;
        case 128: cip = (const int*)(code + pri); break;                       // JUMP_PRI
        case 129: {  // SWITCH
            const int* table = (const int*)Target(*cip, code);
            num = table[1];
            cip = (const int*)Target(table[2], code);
            const int* entry = table + 3;
            if (num <= 0) {
                break;
            }
            while (entry[0] != pri) {
                num--;
                entry += 2;
                if (num <= 0) {
                    break;
                }
            }
            if (num > 0) {
                cip = (const int*)Target(entry[1], code);
            }
            break;
        }
        case 130:  // CASETBL: nothing, and the next word is taken for an opcode
            break;
        default:
            FAIL(kAmxErrInvalidInstruction);
        }
    }
#undef CELL
#undef SET
#undef OPERAND
#undef FAIL
#undef OUT_OF_BOUNDS
}
