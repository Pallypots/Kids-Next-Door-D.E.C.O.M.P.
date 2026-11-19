// What start-up mounts design data from: LIBRARY.ADD names .ADR registries and .ADB database managers, and an .ADB holds field descriptions, record layouts and tables of records.
// The words DataRegistry, DatabaseManager, DataComponentDesc, RecordLayout, DataRecord come from the keyword tables in the executable; every class and member name is inferred.
// Address clusters: 0x0009B8D0-0x0009C1A2 (the library), 0x000B1C20-0x000B2470 (the manager), 0x000B9AA0-0x000BACDE (layouts, tables, records), and 0x0006BDB0.
//
// The disc holds one .ADD, with only a DataRegistry list, and no .ADB at all, so the manager's half here runs on written text alone. See docs/worklog/2026-11-07-add.md.
//
// The registry, the field descriptor, the value classes and the owner are the .ADR work's, in include/at/AtDataRegistry.h. Three members of AtDataString are not recovered: the seams at the end of
// this file stand for them.
#pragma once

#include "at/AtDataRegistry.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

class AtDatabaseManager;
class AtDataRecordDatabase;
class AtRecordLayout;

// vftable 0x002855D8. Which fields a record has, by name. Its map holds the address of each entry of m_fields, not the descriptor.
class AtRecordLayout : public GmNameMap {
public:
    AtRecordLayout();                                  // 0x000B9AC0
    ~AtRecordLayout() override;                        // slot 0; 0x000B1C30 vector deleting, 0x000B9D10 complete
    void Clear() override;                             // slot 1, 0x000B9AE0

    int Read(AtIoTokenizer* tokenizer, GmNameMap* descriptors);  // 0x000B9B20 a name no descriptor has gives a null field
    AtDataDescriptor* FieldAt(unsigned int index);  // 0x000B9AB0

    static void SetHeap(void* heap);                   // 0x000B9AA0

    char m_name[8];                    // +0x14
    char* m_description;               // +0x1C
    unsigned int m_count;              // +0x20
    AtDataDescriptor** m_fields; // +0x24
    int m_loaded;                      // +0x28

    static void* s_heap;               // 0x003ADE90
};

// vftable 0x00285608. One row of a table: its values live in the table's columns.
class AtDataRecord {
public:
    AtDataRecord();                                    // 0x000BAA70
    virtual ~AtDataRecord();                           // slot 0; 0x000B9DF0 vector deleting, 0x000BACD0 complete

    int Set(unsigned int index, AtRecordLayout* layout, AtDataComponent** columns);  // 0x000BAA90 0
    AtDataComponent* FieldAt(unsigned int column);         // 0x000BAAC0 null for a type no table holds
    AtDataDescriptor* DescriptorAt(unsigned int column);  // 0x000BABB0 null for a type no table holds
    AtDataComponent* FieldNamed(const char* name);         // 0x000BAC60 compares eight bytes, and with no match asks for the column past the last

    unsigned int m_index;      // +0x04
    AtRecordLayout* m_layout;  // +0x08
    AtDataComponent** m_columns;   // +0x0C
    int m_inUse;               // +0x10 set for a record the file gave
};

// vftable 0x002855E0. Named records of one layout, stored by column.
class AtDataRecordDatabase : public GmNameMap {
public:
    AtDataRecordDatabase();                            // 0x000B9E60 leaves m_layout, m_records, m_name and m_description: destroying one never read crashes on what the heap left, as in the original
    ~AtDataRecordDatabase() override;                  // slot 0; 0x000BAA50 deleting, 0x000BAA00 complete
    void Clear() override;                             // slot 1, 0x000B9E90 leaves m_records and m_layout, so a second Clear, or destroying after one, fails as in the original
    virtual int Load(AtDatabaseManager* manager, AtRecordLayout* layout, AtIoTokenizer* tokenizer, unsigned int extra, int unused, int fromFile);  // slot 2, 0x000B9DC0
    // Slot 3, 0x000B9F70. fromFile 0 reads no rows: the count is discarded, only `extra` are made, and the closing curlies stay unread.
    // Rows past the file's, `extra` of them, are empty. A field of a type outside 1 to 11 (a Relational, or a description with no Type) crashes the original; here it stops with a diagnostic.
    virtual int Read(AtDatabaseManager* manager, AtIoTokenizer* tokenizer, unsigned int extra, int unused, int fromFile);
    virtual void Reset();                              // slot 4, 0x000B9DA0 frees nothing
    virtual AtDataRecord* RecordAt(unsigned int index);  // slot 5, 0x000B9E50
    virtual AtDataRecord* FindByValue(unsigned int column, const void* value);  // slot 6, 0x000BA9A0 hands back the record whose field equals value. Not reached by loading
    virtual int Unknown7(int);                         // slot 7, 0x000B15B0 folded; 0
    virtual AtDataRecord* TakeUnused(unsigned int* index);  // slot 8, 0x000BA960 the first row not in use, now in use. Not reached by loading
    virtual void OnReady();                            // slot 9, 0x001CC9F0 folded, empty

    static void SetHeap(void* heap);                   // 0x000B9D90

    char m_name[8];              // +0x14
    char* m_description;         // +0x1C
    unsigned int m_count;        // +0x20
    int m_unknown24;             // +0x24 only cleared
    AtDataRecord* m_records;     // +0x28
    int m_unknown2C;             // +0x2C only cleared
    AtRecordLayout* m_layout;    // +0x30
    AtDataComponent** m_columns;     // +0x34 also says it has been read
    char* m_characters;          // +0x38 what string fields point into
    unsigned int m_characterSize;  // +0x3C
    unsigned int m_characterUsed;  // +0x40

    static void* s_heap;         // 0x003ADE94
};

// vftable 0x00284708. An .ADB file. Its own map holds layouts and record tables by name, the second map field descriptions.
class AtDatabaseManager : public GmNameMap {
public:
    AtDatabaseManager();                               // 0x000B1C90 empties the list of managers before it joins, so the list holds only the newest
    ~AtDatabaseManager() override;                     // slot 0; 0x000B2460 deleting, 0x000B22D0 complete
    void Clear() override;                             // slot 1, 0x000B1D30 also empties the list. Does not reset the three counts

    int Load(AtDataRegistryOwner* owner, const char* name, int unused, char* spaceOut, unsigned int extra, int fromFile);  // 0x000B2330 name + ".ADB". Called also from 0x000BCEC0, not recovered
    int Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer, char* spaceOut, unsigned int extra, int fromFile);  // 0x000B1E30
    void* FindEverywhere(const char* name);            // 0x000B2290 here, then along the list

    static void SetHeap(void* heap);                   // 0x000B1C20

    AtDataDescriptor* m_descriptors;  // +0x14
    AtRecordLayout* m_layouts;              // +0x18
    AtDataRecordDatabase** m_tables;        // +0x1C
    GmNameMap m_descriptorsByName;          // +0x20
    unsigned int m_descriptorCount;         // +0x34
    unsigned int m_layoutCount;             // +0x38
    unsigned int m_tableCount;              // +0x3C
    unsigned int m_characterScale;          // +0x40 always 1 here: bytes for a character of a string field
    AtDatabaseManager* m_next;              // +0x44
    int m_loaded;                           // +0x48
    char* m_fileName;                       // +0x4C

    static AtDatabaseManager* s_first;      // 0x003ADE2C
    static void* s_heap;                    // 0x003ADE28
    enum { kKeywordCount = 0x22 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B5310
};

// A .ADD file. Held at offset 0x6B40 of App; start-up calls Load(owner, "Library", 0, parent) from 0x0008DEA0, and 0x0008ED60 calls Clear.
class AtDataLibrary {
public:
    AtDataLibrary();                                   // 0x0009BB10
    ~AtDataLibrary();                                  // 0x0009BD40

    // 0x0009BD50 name + ".ADD". The names are collected first; each is loaded after the file is closed, with the file's directory on the path.
    void Load(AtDataRegistryOwner* owner, const char* name, int unused, AtDataRegistry* parent);
    void Clear();                                      // 0x0009BB40 what it has not freed of the names is lost
    void MakeRegistries();                             // 0x0009B8E0
    void MakeDatabases();                              // 0x0009BA00
    void LoadEntries(int unused, AtDataRegistry* parent);  // 0x0009BC10 and frees the names

    static void SetHeap(void* heap);                   // 0x0009B8D0

    int m_loaded;                      // +0x00
    AtDataRegistryOwner* m_owner;              // +0x04
    AtDataRegistry** m_registries;     // +0x08
    AtDatabaseManager** m_databases;   // +0x0C
    unsigned short m_registryCount;    // +0x10
    unsigned short m_databaseCount;    // +0x12

    static void* s_heap;               // 0x0030593C
    static char** s_registryNames;     // 0x00305938
    static char** s_databaseNames;     // 0x00305934
    enum { kKeywordCount = 2 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B47F0
};

// The table reader's three AtDataString calls (0x0006A1D0, 0x0006A4D0, 0x00069FD0) are the members Forget, BindAndRead and Bind (include/at/AtDataRegistry.h, at/datalookup).

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtRecordLayout, m_name) == 0x14, "AtRecordLayout::m_name");
static_assert(offsetof(AtRecordLayout, m_loaded) == 0x28, "AtRecordLayout::m_loaded");
static_assert(sizeof(AtRecordLayout) == 0x2C, "AtRecordLayout");
static_assert(sizeof(AtDataRecord) == 0x14, "AtDataRecord");
static_assert(offsetof(AtDataRecordDatabase, m_records) == 0x28, "AtDataRecordDatabase::m_records");
static_assert(offsetof(AtDataRecordDatabase, m_columns) == 0x34, "AtDataRecordDatabase::m_columns");
static_assert(sizeof(AtDataRecordDatabase) == 0x44, "AtDataRecordDatabase");
static_assert(offsetof(AtDatabaseManager, m_descriptorsByName) == 0x20, "AtDatabaseManager::m_descriptorsByName");
static_assert(offsetof(AtDatabaseManager, m_next) == 0x44, "AtDatabaseManager::m_next");
static_assert(sizeof(AtDatabaseManager) == 0x50, "AtDatabaseManager");
static_assert(offsetof(AtDataLibrary, m_registryCount) == 0x10, "AtDataLibrary::m_registryCount");
static_assert(sizeof(AtDataLibrary) == 0x14, "AtDataLibrary");
#pragma pop_macro("offsetof")
#endif
