// The registry names: every design data registry the game has loaded and not let go, kept under the name of the file it was read from, so that game objects can find their own
// type's design data by a short name. A level's InstanceRegistries items add each registry they load (GmRegistryItem::Run, 0x001B49B0), start-up adds the library's root registry,
// and others add theirs (calls at 0x0008DFF6, 0x0012FBE9, 0x001304FE, 0x0013DACB, 0x0013DC11, 0x00143F84, 0x0014732F, 0x0017DFF2); the level manifest's Free and others release them.
//
// There is no lookup of its own: game code calls the name map's Find (0x00025150) on s_instance with a name such as "Instance", "SaveData" or a record's type name, and reads the
// registry it gets with AtDataRegistry::ValueAtPath (0x00068860) or ValueNamed (0x00067CB0), which are the design data branch's (datalookup), not this one's.
//
// Original address cluster 0x00071DC0-0x00071FCB. The class and member names are inferred; nothing in the executable names them.
#pragma once
#include "gm/GmNameMap.h"

class AtDataRegistry;

// vftable 0x00277B60. 0x14 bytes, the name map alone. Made by start-up (GmHistoryLogsInit, 0x0006C3E0) with Init(20), and held at +0x04 of the logs.
class GmRegistryNames : public GmNameMap {
public:
    GmRegistryNames();                                       // 0x00071DF0 and becomes s_instance
    ~GmRegistryNames() override;                             // slot 0; 0x00071F50 complete, 0x00071FB0 deleting. s_instance becomes null, whoever it was
    void Clear() override;                                   // slot 1, 0x00071E10 the name map's: a jump the linker folded with other classes' (0x0019C0CE calls it too)

    int Init(unsigned int count);                            // 0x00071E20 clears through slot 1 if it holds a table, then the name map's Init; 0
    // 0x00071E50. The registry's file name, cut at the first ".a" anywhere in it, else the first ".A", ".b" or ".B" (searched for in that order, not by position); then what
    // follows the last backslash, at most eight bytes, with no terminator when eight. The file name is copied into 0x100 bytes on the stack first, with no limit. Does not use this.
    void KeyFor(char* key, const AtDataRegistry* registry);
    void Add(AtDataRegistry* registry);                      // 0x00071F20 under KeyFor's name; a name already here is reported, and added again
    // 0x00071DC0. Under the name the map holds for it; one it does not hold gives an empty name, which the map's Remove then looks for (see GmNameMap::Remove).
    void Release(AtDataRegistry* registry);

    static GmRegistryNames* s_instance;                      // 0x002CD138
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(GmRegistryNames) == 0x14, "GmRegistryNames");
#endif
