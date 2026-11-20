// GmAcActor::GetNoteBank (0x000A1E10, in FaCmCombat's range) and the case-blind compare (0x00013E50, 0x00013F50). Part of gm/combat; in a file of their own so that a caller of
// these alone (the actor's animation callback calls GetNoteBank) does not link the update and all it needs. Names inferred.
#include "gm/FaCmCombat.h"

void AtCopyName8(char* to, const char* from);   // 0x00013F70 eight bytes, or an empty name if `from` is null

namespace {
// The C library's tolower, in the C locale. The original's indexes its character table by the signed character, so a character above 0x7F reads before the table: not reproduced.
int LowerCase(int c) { return c >= 'A' && c <= 'Z' ? c + 0x20 : c; }

}  // namespace

int AtCompareNoCaseN(const char* a, const char* b, int n) {
    if (a == 0) {
        a = "";
    }
    if (b == 0) {
        b = "";
    }
    while (n != 0) {
        const char x = *a;
        n--;
        if (x != *b) {
            if (LowerCase((signed char)*b) != LowerCase((signed char)x)) {
                return (signed char)*a - (signed char)*b;
            }
        } else if (x == 0) {
            return 0;
        }
        a++;
        b++;
    }
    return 0;
}

int AtCompareNoCase8(const char* a, const char* b) {
    return AtCompareNoCaseN(a, b, 8);
}

// ---- GmAcActor's one here ---------------------------------------------------------------------------------------------------
void GmAcActor::GetNoteBank(char* out) {
    AtCopyName8(out, m_noteBank);
}
