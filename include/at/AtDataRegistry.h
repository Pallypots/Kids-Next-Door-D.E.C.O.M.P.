// The design data registry: .ADR files. A registry declares how many values of each type it will hold, then a list of named values ("data links") arranged in a tree by backslash paths; a
// registry may have a parent, whose tree is searched first and under whose root it hangs. Original address cluster 0x00066110-0x0006C240. The words DataRegistry, DataLink, DataComponent come from the
// files; every class and member name is inferred.
//
// Loading a file reaches the value classes' slots 0 to 4. Slots 5 to 9 are how the game reads and writes values afterwards (src/at/AtDataValueAccess.cpp), and the lookups by path and name
// are in src/at/AtDataLookup.cpp.
#pragma once

#include "at/AtIoTokenizer.h"
#include "at/AtText.h"
#include "gm/GmNameMap.h"

class AtDataComponent;
class AtDataRegistry;
class AtDataRegistryNameSpaces;

// vftable 0x0027793C, one slot. What the file said about one value: its type, how many, and its bounds.
class AtDataDescriptor {
public:
    enum Flags { kTyped = 1, kAcceptsParentData = 4 };

    AtDataDescriptor();                                // 0x0006BC10 type 13, count 1, the widest bounds
    virtual ~AtDataDescriptor();                       // slot 0; 0x0006BFB0 complete, 0x00066620 vector deleting

    void Clear();                                      // 0x0006BC60 back to what the constructor made; the format is freed
    int SetType(unsigned int type, unsigned int count);    // 0x0006BCB0 clears first if typed already; 0
    int SetIntBounds(int floor, int ceiling);          // 0x0006BB20 0
    int SetRealBounds(float floor, float ceiling);     // 0x0006BB40 -1 and nothing done unless the type holds reals
    int GetIntBounds(int* floor, int* ceiling);        // 0x0006BAD0 0
    int SetFormat(const char* format);                 // 0x0006BB70 frees any before. Allocates its length with no room for the terminator, and copies the terminator: as the original does
    // 0x0006BDB0, the .ADB reader of a DataComponentDesc (src/at/AtDatabaseManager.cpp, with that format's words). Skips to the first left curly, whatever comes before it. A file that ends first never
    // returns, as in the original.
    int Read(AtIoTokenizer* tokenizer);
    int CopyFrom(const AtDataDescriptor* other);       // 0x0006BCE0 clears first if typed; the format allocated one byte short, as SetFormat; typed afterwards; 0

    static void SetHeap(void* heap);                   // 0x0006BAC0

    char m_name[8];            // +0x04 cleared as two words here; Read (0x0006BDB0) shows it is the name. Not always terminated. An .ADR descriptor has none
    unsigned int m_type;       // +0x0C AtDataRegistry::Type; 13 untyped. An .ADB Relational gives 12, which no table can hold
    unsigned int m_flags;      // +0x10
    unsigned int m_count;      // +0x14 elements, for the array types
    int m_intFloor;            // +0x18
    int m_intCeiling;          // +0x1C for a string, the longest it can be
    float m_realFloor;         // +0x20
    float m_realCeiling;       // +0x24
    char* m_format;            // +0x28

    static void* s_heap;       // 0x002CB0A0
};

// vftable 0x00277940. A named node of the tree. A node with no value is a registry's root, and a search passes through it into its children.
class AtDataLink {
public:
    AtDataLink();                                      // 0x0006BFD0
    virtual ~AtDataLink();                             // slot 0; 0x0006C240 complete, 0x00066680 vector deleting
    virtual AtDataRegistry* Registry();                // slot 1, 0x0006BFF0 its value's registry, or null

    int Set(const char* name, AtDataComponent* value, AtDataLink* parent);  // 0x0006C160 clears first if in use; a null name stays null; the last of the parent's children; 0
    void Clear();                                      // 0x0006C110 detaches it; its children keep each other but lose their parent
    void AttachTo(AtDataLink* parent);                 // 0x0006C000 the last child. Does not leave any parent it had
    void Detach();                                     // 0x0006C030
    // 0x0006C080 appends "A\B\this\" from the first named link up, stopping at a link with no name: a registry's root. A nameless link here is
    // passed over to its parent if skipNameless; an empty name adds nothing, and neither does anything above it. No bound on out.
    void AppendPath(char* out, int skipNameless);

    static void SetHeap(void* heap);                   // 0x0006BFC0

    AtDataComponent* m_value;  // +0x04
    char* m_name;              // +0x08
    AtDataLink* m_parent;      // +0x0C
    AtDataLink* m_sibling;     // +0x10
    AtDataLink* m_firstChild;  // +0x14
    int m_inUse;               // +0x18

    static void* s_heap;       // 0x002CB0A4
};

// vftable 0x0027760C. A registry's root, which knows its registry without a value.
class AtDataLinkRoot : public AtDataLink {
public:
    ~AtDataLinkRoot() override;                        // slot 0; 0x00066750 complete, 0x00066760 deleting
    AtDataRegistry* Registry() override;               // slot 1, 0x0010F4C0

    AtDataRegistry* m_registry; // +0x1C
};

// 0x20 bytes. A block of values of one type, with each value's descriptor and link in step, and further blocks chained on when it fills.
class AtDataComponentPool {
public:
    AtDataComponent* ElementAt(unsigned int index);    // 0x00066110 index is not reduced along the chain: it is measured against each block's own count, and used in the block that takes it
    void Allocate();                                   // 0x00066B30 if counted and not yet allocated
    void Clear();                                      // 0x00066930 and the chain
    AtDataLink* FindByName(const char* name, int exact);  // 0x00066A70 the whole name, ignoring case unless exact; exact does not carry on to the chain
    AtDataLink* New(int unused);                       // 0x00067430 the first link not in use; a block with no count becomes one of four, and a full chain grows
    void Grow();                                       // 0x000666E0 an empty block of the same count and type
    void CopyFrom(AtDataComponentPool* other, AtDataRegistry* registry);  // 0x00067200 the first block of other's, every link under registry's root; a chained block is not copied
    void Relink(AtDataComponentPool* other, AtDataRegistry* registry);    // 0x00068720 each copied link's parent, found by other's parent's whole path; sets m_parent only, not the lists

    unsigned int m_count;      // +0x00
    unsigned int m_capacity;   // +0x04 set with the count, and copied to a new block
    unsigned int m_type;       // +0x08
    unsigned int m_used;       // +0x0C
    AtDataComponent* m_values; // +0x10 of the type's own class, whose size differs
    AtDataDescriptor* m_descriptors;  // +0x14
    AtDataLink* m_links;       // +0x18
    AtDataComponentPool* m_next;  // +0x1C
};

// The registries in one name space: list through AtDataRegistry::m_next, 0x14 bytes. 0x000603A0, 0x000B3F70, 0x000602C0 are the same shape for other lists in the owner, whose links sit elsewhere.
class AtDataRegistryGroup {
public:
    AtDataRegistryGroup();                             // 0x000603A0
    void Reset();                                      // 0x000B3F70
    void SetName(const char* name);                    // 0x000602C0
    void Add(AtDataRegistry* registry);                // 0x000603B0 at the head; leaves any group it was in
    void Remove(AtDataRegistry* registry);             // 0x000602E0 nothing if it is not here. An emptied group goes back to its database

    char m_name[8];            // +0x00
    AtDataRegistry* m_first;   // +0x08
    AtDataRegistryGroup* m_nextFree;  // +0x0C
    AtDataRegistryNameSpaces* m_database;  // +0x10
};

// vftable 0x00277424. Groups by name, from a fixed pool: the same design as GmNameSpaceDb, for registries.
class AtDataRegistryNameSpaces : public GmNameMap {
public:
    AtDataRegistryNameSpaces();                        // 0x0005FFF0
    ~AtDataRegistryNameSpaces() override;              // slot 0; 0x00060230 complete, 0x000602A0 deleting
    void Clear() override;                             // slot 1, 0x000B40E0

    void Init(unsigned int count);                     // 0x00060010
    void Release(AtDataRegistryGroup* group);          // 0x0005FFC0 back to the pool
    void Add(const char* space, AtDataRegistry* registry);  // 0x00060110 reports running out of the pool, then crashes on it in the original
    static void Remove(AtDataRegistry* registry);      // 0x00060170

    AtDataRegistryGroup* m_pool;  // +0x14
    unsigned int m_poolSize;   // +0x18
    AtDataRegistryGroup* m_free;  // +0x1C
    AtDataRegistryGroup* m_last;  // +0x20

    static void SetHeap(void* heap);                   // 0x0005FFB0
    static void* s_heap;       // 0x002C9F9C
};

// The class with vftable 0x00277420, of which only what the loaders use is recovered: the object at offset 0x394 of the class at 0x003B357C, which start-up hands to the .ADD library. Its
// constructor (near 0x0005EFF0) and other members are not; these members' own constructors stand in. Its other tables and adds (0x0005F0F0, 0x0005F130) take other classes into the same members, by
// links elsewhere. The .ADR and .ADD work named it twice (AtDataRegistryOwner, AtDataOwner): the database groups at +0x08 and +0x20 run with no gap into the registry groups at +0x44 and +0x58.
struct AtDataRegistryOwner {
    void Add(AtDataRegistry* registry, const char* space);  // 0x0005F0B0 a null or empty name means the plain group
    void Remove(AtDataRegistry* registry);             // 0x0005F1F0
    void AddDatabase(GmNameMap* map, const char* space);  // 0x000992D0 a null or empty name means the plain group

    void* m_unknown00;                  // +0x00
    int m_unknown04;                    // +0x04
    GmNameMapGroup m_databases;         // +0x08 .ADB managers with no name space
    GmNameSpaceDb m_databaseSpaces;     // +0x20
    AtDataRegistryGroup m_plain;        // +0x44
    AtDataRegistryNameSpaces m_spaces;  // +0x58
};

// vftable 0x00277614, one slot.
class AtDataRegistry {
public:
    enum Type { kEmpty, kBoolean, kInt, kReal, kShortName, kString, kBooleanArray, kByteArray, kIntArray, kRealArray, kShortNameArray, kStringArray, kTypeCount = 13 };

    AtDataRegistry();                                  // 0x000674E0
    virtual ~AtDataRegistry();                         // slot 0; 0x00067C50 complete, 0x000687B0 deleting

    int Load(AtDataRegistryOwner* owner, const char* name, int unused, char* spaceOut, AtDataRegistry* parent);  // 0x00068880 name + ".ADR"
    int Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer, char* spaceOut, AtDataRegistry* parent);  // 0x00067D80
    void Clear();                                      // 0x00067540 if not loaded, only m_unknown38 goes
    void Reset();                                      // 0x00066780 frees nothing
    AtDataLink* FindChild(const char* path, AtDataLink* under);  // 0x00067980 path's first part, among under's children or the root's, down through nodes with no value
    int NameMatches(const char* name, const char* path);  // 0x00066800 equal up to the end or a backslash on each
    void Clamp(unsigned int type, AtDataComponent* value);  // 0x00067A60 into the descriptor's bounds, by the type given, not the value's own

    // Reading it: src/at/AtDataLookup.cpp.
    // 0x000679E0 the search every game object's record reader calls. Part by part from from's children (the root's if null), each part through FindChild.
    // With climb, a part not among from's children is looked for among its parent's, and so on up; one found nowhere leaves null, and the next part is
    // then looked for from the root. A null or empty path answers from.
    AtDataLink* FindPath(AtDataLink* from, const char* path, int climb);
    AtDataLink* FindLink(const char* name, AtDataLink* under, int exact);  // 0x000678A0 the whole name, depth first from under (the root if null) through children and then siblings, so past this registry's tree into those hung beside it
    AtDataLink* FindInPools(const char* name, int exact);  // 0x00067950 FindByName in each block, by type
    AtDataComponent* ValueNamed(const char* name);     // 0x00067CB0 FindByName in each block, ignoring case; the value
    AtDataComponent* ValueAt(AtDataLink* from, const char* path, int climb);  // 0x00067CF0 FindPath's value
    AtDataLink* FindFromRoot(const char* path, int climb);  // 0x00067D20 FindPath from the root
    AtDataComponent* ValueAtPath(const char* path, int climb);  // 0x00068860 FindPath's value, from the root
    // 0x000687D0 the other loading form: every block copied from other (CopyFrom), then the parents (Relink), filed with other's owner in the plain group.
    // No parent registry, file name or count of types is taken. 0
    int CopyFrom(AtDataRegistry* other);
    int AttachUnder(AtDataRegistry* parent, const char* path);  // 0x00067D40 this root under parent's link at path, or its root if path is null; 0 and nothing done if not found, else 1
    // 0x000675B0 empty blocks of the counts given, all allocated, under parent if there is one, and filed in the plain group. Start-up makes the registry every
    // loaded one hangs under this way, with no values. The block of no type, the thirteenth, is not touched. 0
    int Prepare(AtDataRegistryOwner* owner, unsigned int empties, unsigned int booleans, unsigned int ints, unsigned int reals, unsigned int shortNames, unsigned int strings,
               unsigned int booleanArrays, unsigned int byteArrays, unsigned int intArrays, unsigned int realArrays, unsigned int shortNameArrays, unsigned int stringArrays,
               AtDataRegistry* parent);
    void SetFileName(const char* name);                // 0x00066860 into the block there if it is long enough

    static void SetHeap(void* heap);                   // 0x00066100

    int m_unknown04;           // +0x04 never touched
    int m_unknown08;           // +0x08 never touched
    AtDataLinkRoot m_root;     // +0x0C
    AtDataRegistryOwner* m_owner;  // +0x2C cleared by a reload
    int m_unknown30;           // +0x30 never touched
    AtDataRegistry* m_parent;  // +0x34
    void* m_unknown38;         // +0x38 freed here, set elsewhere
    AtDataComponentPool m_pools[kTypeCount];  // +0x3C each has its index as its type
    unsigned int m_componentTypes;  // +0x1DC how many in the file
    AtDataRegistryGroup* m_group;  // +0x1E0
    AtDataRegistry* m_next;    // +0x1E4
    char* m_fileName;          // +0x1E8
    int m_loaded;              // +0x1EC

    static void* s_heap;       // 0x002CB094 set once
    static void* s_poolHeap;   // 0x002CB098 copied from s_heap by each read
};

// vftable 0x00277754. A value of no type. What follows a value's class in its pool is only its own class, so the sizes below are what the pools index by.
class AtDataComponent {
public:
    AtDataComponent();                                 // 0x00069DF0
    virtual ~AtDataComponent();                        // slot 0; 0x00069E10 complete, 0x000661A0 vector deleting
    virtual int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer);  // slot 1, 0x00069E20 nothing to read
    virtual int SetDescriptor(AtDataDescriptor* descriptor);  // slot 2, 0x00069E30 0
    virtual void Reset();                              // slot 3, 0x000E7880
    // Slots 4 to 9: src/at/AtDataValueAccess.cpp. On this class the original's are stubs shared with other classes of the game: 0x000B15B0 answers 0,
    // 0x001ACD70 and 0x000CBF30 do nothing.
    virtual int Resize(unsigned int count);            // slot 4, 0x000B15B0 0
    virtual void* ElementAt(unsigned int index);       // slot 5, 0x000B15B0 null
    virtual void SetElement(const void* from, unsigned int index, unsigned int count);  // slot 6, 0x001ACD70 nothing. No bound in any class
    virtual int Equals(const AtDataComponent* other);  // slot 7, 0x000B15B0 0. Other is taken to be of this one's class
    virtual void RemoveAt(unsigned int index);         // slot 8, 0x000CBF30 nothing
    virtual void Write(AtIoTokenizer* tokenizer);      // slot 9, 0x00069E40 the value as text; here only a space and the word WillAcceptParentData, if m_flags has kAcceptsParentData

    AtDataDescriptor* m_descriptor;  // +0x04
    AtDataRegistry* m_registry;      // +0x08 set by the registry that read it

    static void SetHeap(void* heap);                   // 0x00069DE0
    static void* s_heap;             // 0x002CB09C
};

// vftable 0x0027777C.
class AtDataBoolean : public AtDataComponent {
public:
    AtDataBoolean();                                   // 0x0006A040
    ~AtDataBoolean() override;                         // slot 0; 0x00069E70 complete, 0x00066200 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006A080
    void Reset() override;                             // slot 3, 0x00069F50
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A010 the value, whatever the index
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A000 the first word, whatever the index and count
    int Equals(const AtDataComponent* other) override; // slot 7, 0x0006A0A0 1 if the two values are the same word; other is taken to be of this class. Reached from AtDataRecordDatabase::FindByValue
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x00069EB0
    int Set(AtDataDescriptor* descriptor, int value);  // 0x0006A060 0
    int m_value;               // +0x0C
};

// vftable 0x002777A4. Unlike the others its constructor leaves the value.
class AtDataInt : public AtDataComponent {
public:
    AtDataInt();                                       // 0x00069E80
    ~AtDataInt() override;                             // slot 0; 0x00069EA0 complete, 0x00066260 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006A080 folded with AtDataBoolean's
    void Reset() override;                             // slot 3, 0x00069F50 folded
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A010 folded
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A000 folded
    int Equals(const AtDataComponent* other) override; // slot 7, 0x0006A0A0 folded with AtDataBoolean's
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x00069EB0 folded
    int Set(AtDataDescriptor* descriptor, int value);  // 0x0006A060 folded
    int m_value;               // +0x0C
};

// vftable 0x002777CC.
class AtDataReal : public AtDataComponent {
public:
    AtDataReal();                                      // 0x00069ED0
    ~AtDataReal() override;                            // slot 0; 0x00069EF0 complete, 0x000662C0 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006A0C0
    void Reset() override;                             // slot 3, 0x00069F50 folded
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A010 folded
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A000 folded: the word as it is
    int Equals(const AtDataComponent* other) override; // slot 7, 0x0006A0E0 equal as reals
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x00069F00
    int Set(AtDataDescriptor* descriptor, float value);  // 0x0006A060 folded
    float m_value;             // +0x0C
};

// vftable 0x002777F4.
class AtDataShortName : public AtDataComponent {
public:
    AtDataShortName();                                 // 0x00069F20
    ~AtDataShortName() override;                       // slot 0; 0x00069F40 complete, 0x00066320 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x00069F60
    void Reset() override;                             // slot 3, 0x00069F50 folded
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A010 folded
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A020 eight bytes
    int Equals(const AtDataComponent* other) override; // slot 7, 0x0006A130 up to eight characters or a terminator
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x00069FA0
    int Set(AtDataDescriptor* descriptor, const char* name);  // 0x0006A100 eight bytes
    char m_value[8];           // +0x0C not always terminated
};

// vftable 0x0027781C.
class AtDataString : public AtDataComponent {
public:
    AtDataString();                                    // 0x0006A170
    ~AtDataString() override;                          // slot 0; 0x0006A660 complete, 0x00066380 vector deleting
    // Slot 1, 0x0006A6F0. A second string after the first says Unicode if it is that word, ignoring case. The tokenizer holds one string at a time, so the value is then that second string.
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;
    void Reset() override;                             // slot 3, 0x0006A200

    // Slots 5 and 6 are the base's stubs. Slot 7 compares what slot 5 answers, null: unequal lengths are unequal, two empty texts equal, and two others
    // of one length crash on null, as in the original.
    int Equals(const AtDataComponent* other) override; // slot 7, 0x0006A540
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006A5C0 narrowed

    int Assign(AtDataDescriptor* descriptor, const char* text);  // 0x0006A330 room for the text and a terminator, or the ceiling if larger. Wide text clears only half its block
    int CopyText(AtDataDescriptor* descriptor, AtText* other);  // 0x0006A250 always wide: room for other's capacity, or the ceiling if larger
    // The .ADB tables' strings, whose characters are the table's own (src/at/AtDatabaseManager.cpp still reaches these through the seams in AtDataLibrary.h).
    void Forget();                                     // 0x0006A1D0 lets go of the characters without freeing them; the base's fields stay
    int BindAndRead(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer, char* characters, unsigned int size);  // 0x0006A4D0 narrow, in characters; m_chars not set
    int Bind(AtDataDescriptor* descriptor, char* characters, unsigned int size);  // 0x00069FD0 characters as they are; nothing held before is let go

    AtTextAscii m_narrow;      // +0x0C
    AtTextUnicode m_wide;      // +0x20
    int m_unicode;             // +0x34
    void* m_chars;             // +0x38
};

// vftable 0x0027784C.
class AtDataBooleanArray : public AtDataComponent {
public:
    AtDataBooleanArray();                              // 0x0006A810
    ~AtDataBooleanArray() override;                    // slot 0; 0x0006B710 complete, 0x000663E0 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006A920
    void Reset() override;                             // slot 3, 0x0006A840
    int Resize(unsigned int count) override;           // slot 4, 0x0006A870 keeps what fits, and sets the descriptor's count; -4 with no memory
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A830 element index, no bound
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A790 count elements from index on, no bound
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006ADE0 the rest moved down, then Resize to one fewer whatever the index
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006A970 each followed by a space
    int Copy(AtDataDescriptor* descriptor, const AtDataBooleanArray* other);  // 0x0006AD70 SetData with other's count and elements
    int SetData(AtDataDescriptor* descriptor, unsigned int count, const int* data);  // 0x0006AC80 what was held goes through Reset; the descriptor's count is not set
    int* m_data;               // +0x0C
};

// vftable 0x00277874.
class AtDataByteArray : public AtDataComponent {
public:
    AtDataByteArray();                                 // 0x0006A9C0
    ~AtDataByteArray() override;                       // slot 0; 0x0006B780 complete, 0x00066440 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006AB40 keeps the low byte
    void Reset() override;                             // slot 3, 0x0006A840 folded
    int Resize(unsigned int count) override;           // slot 4, 0x0006A9F0
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A9E0 element index, no bound
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A760 count elements from index on, no bound
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006AC00 the rest moved down, then Resize to one fewer whatever the index
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006ABB0 each followed by a space
    int Copy(AtDataDescriptor* descriptor, const AtDataByteArray* other);  // 0x0006AB90 SetData with other's count and elements
    int SetData(AtDataDescriptor* descriptor, unsigned int count, const unsigned char* data);  // 0x0006AAA0 what was held goes through Reset; the descriptor's count is not set
    unsigned char* m_data;     // +0x0C
};

// vftable 0x0027789C.
class AtDataIntArray : public AtDataComponent {
public:
    AtDataIntArray();                                  // 0x0006AC40
    ~AtDataIntArray() override;                        // slot 0; 0x0006B7F0 complete, 0x000664A0 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006A920 folded
    void Reset() override;                             // slot 3, 0x0006A840 folded
    int Resize(unsigned int count) override;           // slot 4, 0x0006A870 folded
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A830 folded element index, no bound
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A790 folded count elements from index on, no bound
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006ADE0 folded the rest moved down, then Resize to one fewer whatever the index
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006A970 folded each followed by a space
    int Copy(AtDataDescriptor* descriptor, const AtDataIntArray* other);  // 0x0006AD70 folded SetData with other's count and elements
    int SetData(AtDataDescriptor* descriptor, unsigned int count, const int* data);  // 0x0006AC80 folded what was held goes through Reset; the descriptor's count is not set
    int* m_data;               // +0x0C
};

// vftable 0x002778C4.
class AtDataRealArray : public AtDataComponent {
public:
    AtDataRealArray();                                 // 0x0006AC60
    ~AtDataRealArray() override;                       // slot 0; 0x0006B860 complete, 0x00066500 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006AD20
    void Reset() override;                             // slot 3, 0x0006A840 folded
    int Resize(unsigned int count) override;           // slot 4, 0x0006A870 folded
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006A830 folded element index, no bound
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A790 folded count elements from index on, no bound
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006ADE0 folded the rest moved down, then Resize to one fewer whatever the index
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006AD90 each followed by a space
    int Copy(AtDataDescriptor* descriptor, const AtDataRealArray* other);  // 0x0006AD70 folded SetData with other's count and elements
    int SetData(AtDataDescriptor* descriptor, unsigned int count, const float* data);  // 0x0006AC80 folded what was held goes through Reset; the descriptor's count is not set
    float* m_data;             // +0x0C
};

// vftable 0x002778EC.
class AtDataShortNameArray : public AtDataComponent {
public:
    AtDataShortNameArray();                            // 0x0006AE30
    ~AtDataShortNameArray() override;                  // slot 0; 0x0006B8D0 complete, 0x00066560 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006AFB0
    void Reset() override;                             // slot 3, 0x0006A840 folded
    int Resize(unsigned int count) override;           // slot 4, 0x0006AE60
    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006AE50 element index, no bound
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A7B0 count elements from index on, no bound
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006B080 the rest moved down, then Resize to one fewer whatever the index
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006B030 each followed by a space
    int Copy(AtDataDescriptor* descriptor, const AtDataShortNameArray* other);  // 0x0006B010 SetData with other's count and elements
    int SetData(AtDataDescriptor* descriptor, unsigned int count, char (*data)[8]);  // 0x0006AF10 what was held goes through Reset; the descriptor's count is not set
    char (*m_data)[8];         // +0x0C
};

// vftable 0x00277914. Each element is wide text, whatever the file's text was.
class AtDataStringArray : public AtDataComponent {
public:
    AtDataStringArray();                               // 0x0006B0D0
    ~AtDataStringArray() override;                     // slot 0; 0x0006B940 complete, 0x000665C0 vector deleting
    int Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) override;  // slot 1, 0x0006B990
    void Reset() override;                             // slot 3, 0x0006B100 with nothing held it does not clear the base's fields either
    int Resize(unsigned int count) override;           // slot 4, 0x0006B170 -4 with no memory, having kept what it had

    void* ElementAt(unsigned int index) override;      // slot 5, 0x0006B0F0 element index's characters
    void SetElement(const void* from, unsigned int index, unsigned int count) override;  // slot 6, 0x0006A7D0 from's characters, unterminated, over the list of pointers from index on; count unused
    void RemoveAt(unsigned int index) override;        // slot 8, 0x0006B6A0 the rest moved down, then Resize to one fewer; the removed characters are not freed
    void Write(AtIoTokenizer* tokenizer) override;     // slot 9, 0x0006B5C0 each narrowed, followed by a space

    int SetText(unsigned int index, const char* text);  // 0x0006B4F0 does not free what was at index
    int Copy(AtDataDescriptor* descriptor, const AtDataStringArray* other);  // 0x0006B9E0 through a block on the token file heap as long as other's roomiest text; reads other's first text even when it has none
    int Make(AtDataDescriptor* descriptor, unsigned int size, unsigned int count);  // 0x0006B330 count empty texts of size characters; SetText later loses these blocks

    unsigned short** m_chars;  // +0x0C
    AtTextUnicode* m_texts;    // +0x10
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtDataDescriptor, m_name) == 0x04, "AtDataDescriptor::m_name");
static_assert(offsetof(AtDataDescriptor, m_intCeiling) == 0x1C, "AtDataDescriptor::m_intCeiling");
static_assert(offsetof(AtDataDescriptor, m_format) == 0x28, "AtDataDescriptor::m_format");
static_assert(sizeof(AtDataDescriptor) == 0x2C, "AtDataDescriptor");
static_assert(offsetof(AtDataLink, m_inUse) == 0x18, "AtDataLink::m_inUse");
static_assert(sizeof(AtDataLink) == 0x1C, "AtDataLink");
static_assert(sizeof(AtDataLinkRoot) == 0x20, "AtDataLinkRoot");
static_assert(sizeof(AtDataComponentPool) == 0x20, "AtDataComponentPool");
static_assert(sizeof(AtDataRegistryGroup) == 0x14, "AtDataRegistryGroup");
static_assert(sizeof(AtDataRegistryNameSpaces) == 0x24, "AtDataRegistryNameSpaces");
static_assert(offsetof(AtDataRegistryOwner, m_databases) == 0x08, "AtDataRegistryOwner::m_databases");
static_assert(offsetof(AtDataRegistryOwner, m_databaseSpaces) == 0x20, "AtDataRegistryOwner::m_databaseSpaces");
static_assert(offsetof(AtDataRegistryOwner, m_plain) == 0x44, "AtDataRegistryOwner::m_plain");
static_assert(offsetof(AtDataRegistryOwner, m_spaces) == 0x58, "AtDataRegistryOwner::m_spaces");
static_assert(offsetof(AtDataRegistry, m_root) == 0x0C, "AtDataRegistry::m_root");
static_assert(offsetof(AtDataRegistry, m_owner) == 0x2C, "AtDataRegistry::m_owner");
static_assert(offsetof(AtDataRegistry, m_parent) == 0x34, "AtDataRegistry::m_parent");
static_assert(offsetof(AtDataRegistry, m_pools) == 0x3C, "AtDataRegistry::m_pools");
static_assert(offsetof(AtDataRegistry, m_componentTypes) == 0x1DC, "AtDataRegistry::m_componentTypes");
static_assert(offsetof(AtDataRegistry, m_loaded) == 0x1EC, "AtDataRegistry::m_loaded");
static_assert(sizeof(AtDataRegistry) == 0x1F0, "AtDataRegistry");
static_assert(sizeof(AtDataComponent) == 0x0C, "AtDataComponent");
static_assert(sizeof(AtDataBoolean) == 0x10, "AtDataBoolean");
static_assert(sizeof(AtDataInt) == 0x10, "AtDataInt");
static_assert(sizeof(AtDataReal) == 0x10, "AtDataReal");
static_assert(sizeof(AtDataShortName) == 0x14, "AtDataShortName");
static_assert(offsetof(AtDataString, m_wide) == 0x20, "AtDataString::m_wide");
static_assert(offsetof(AtDataString, m_chars) == 0x38, "AtDataString::m_chars");
static_assert(sizeof(AtDataString) == 0x3C, "AtDataString");
static_assert(sizeof(AtDataBooleanArray) == 0x10, "AtDataBooleanArray");
static_assert(sizeof(AtDataByteArray) == 0x10, "AtDataByteArray");
static_assert(sizeof(AtDataIntArray) == 0x10, "AtDataIntArray");
static_assert(sizeof(AtDataRealArray) == 0x10, "AtDataRealArray");
static_assert(sizeof(AtDataShortNameArray) == 0x10, "AtDataShortNameArray");
static_assert(sizeof(AtDataStringArray) == 0x14, "AtDataStringArray");
#pragma pop_macro("offsetof")
#endif
