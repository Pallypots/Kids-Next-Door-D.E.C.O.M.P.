// See include/at/AtDataLibrary.h: .ADB files, of which this disc has none.
#include "at/AtDataLibrary.h"

#include "AtIoJamCommon.h"
#include "AtArray.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void AtCopyName8(char* to, const char* from);

void* AtDatabaseManager::s_heap;
AtDatabaseManager* AtDatabaseManager::s_first;
void* AtRecordLayout::s_heap;
void* AtDataRecordDatabase::s_heap;

const AtIoKeyword AtDatabaseManager::s_keywords[AtDatabaseManager::kKeywordCount] = {
    {0x39, "DataComponentDescs"}, {0x3A, "DataComponentDesc"}, {0x3B, "RecordLayoutDatabases"}, {0x3C, "RecordLayoutDatabase"}, {0x3D, "DataRecordDatabases"},
    {0x3E, "DataRecordDatabase"}, {0x3F, "RecordLayout"}, {0x40, "Name"}, {0x41, "Type"}, {0x42, "Flags"}, {0x43, "NumElements"}, {0x44, "Floor"}, {0x45, "Ceiling"},
    {0x46, "FormatString"}, {0x47, "Desc"}, {0x48, "DataComponents"}, {0x49, "DataRecords"}, {0x4A, "Boolean"}, {0x4B, "Byte"}, {0x4C, "Int"}, {0x4D, "Real"},
    {0x4E, "ShortName"}, {0x4F, "String"}, {0x50, "BooleanArray"}, {0x51, "ByteArray"}, {0x52, "IntArray"}, {0x53, "RealArray"}, {0x54, "ShortNameArray"},
    {0x55, "StringArray"}, {0x56, "Relational"}, {0x57, "Database"}, {0x58, "RelatedField"}, {0x59, "ReferencedField"}, {0x5A, "NameSpace"},
};

namespace {

enum Word {
    kDescriptors = 0x39, kDescriptor = 0x3A, kLayouts = 0x3B, kLayout = 0x3C, kTables = 0x3D, kTable = 0x3E, kRecordLayout = 0x3F, kName = 0x40, kType = 0x41, kNumElements = 0x43,
    kFloor = 0x44, kCeiling = 0x45, kFormatString = 0x46, kDesc = 0x47, kDataComponents = 0x48, kDataRecords = 0x49, kBoolean = 0x4A, kInt = 0x4C, kReal = 0x4D, kShortName = 0x4E,
    kString = 0x4F, kBooleanArray = 0x50, kByteArray = 0x51, kIntArray = 0x52, kRealArray = 0x53, kShortNameArray = 0x54, kStringArray = 0x55, kRelational = 0x56, kNameSpace = 0x5A
};

enum Type { kTypeReal = 3, kTypeString = 5, kTypeRealArray = 9, kTypeStringArray = 11, kTypeRelational = 12 };

void Expect(AtIoTokenizer* tokenizer, int word) {
    if (tokenizer->Next() != word) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
}

void* NewFrom(void* heap, unsigned int size) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    void* block = operator new(size);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}

void CopyOnHeap(void* heap, const char* text, char** out) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    *out = (char*)operator new(length + 1);
    AtPopHeap();
    if (*out == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i <= length; i++) {
        (*out)[i] = text[i];
    }
}

bool IsReal(unsigned int type) {
    return type == kTypeReal || type == kTypeRealArray;
}

bool IsValueType(unsigned int type) {
    return type >= 1 && type <= kTypeStringArray;
}

// The size of each value class, by type: what the original's switches step a column by.
unsigned int ValueSize(unsigned int type) {
    switch (type) {
        case 1: return sizeof(AtDataBoolean);
        case 2: return sizeof(AtDataInt);
        case 3: return sizeof(AtDataReal);
        case 4: return sizeof(AtDataShortName);
        case 5: return sizeof(AtDataString);
        case 6: return sizeof(AtDataBooleanArray);
        case 7: return sizeof(AtDataByteArray);
        case 8: return sizeof(AtDataIntArray);
        case 9: return sizeof(AtDataRealArray);
        case 10: return sizeof(AtDataShortNameArray);
        default: return sizeof(AtDataStringArray);
    }
}

// new AtDataBoolean[count] and so on, the eleven cases of 0x000B9F70; null if there was no memory. The heap's push and pop are the caller's.
AtDataComponent* NewColumn(unsigned int type, unsigned int count) {
    switch (type) {
        case 1: return AtNewArray<AtDataBoolean>(count);
        case 2: return AtNewArray<AtDataInt>(count);
        case 3: return AtNewArray<AtDataReal>(count);
        case 4: return AtNewArray<AtDataShortName>(count);
        case 5: return AtNewArray<AtDataString>(count);
        case 6: return AtNewArray<AtDataBooleanArray>(count);
        case 7: return AtNewArray<AtDataByteArray>(count);
        case 8: return AtNewArray<AtDataIntArray>(count);
        case 9: return AtNewArray<AtDataRealArray>(count);
        case 10: return AtNewArray<AtDataShortNameArray>(count);
        default: return AtNewArray<AtDataStringArray>(count);
    }
}

// The original's is one delete[] through the first value's vftable, which C++ does not promise for a base pointer (see AtDataComponentPool::Clear). Here it goes by the type the column was made for:
// its field's, which nothing changes while the table holds it. A column of no rows is freed without asking a value, as delete[] does.
void DeleteColumn(unsigned int type, AtDataComponent* values) {
    switch (type) {
        case 1: AtDeleteArray(static_cast<AtDataBoolean*>(values)); break;
        case 2: AtDeleteArray(static_cast<AtDataInt*>(values)); break;
        case 3: AtDeleteArray(static_cast<AtDataReal*>(values)); break;
        case 4: AtDeleteArray(static_cast<AtDataShortName*>(values)); break;
        case 5: AtDeleteArray(static_cast<AtDataString*>(values)); break;
        case 6: AtDeleteArray(static_cast<AtDataBooleanArray*>(values)); break;
        case 7: AtDeleteArray(static_cast<AtDataByteArray*>(values)); break;
        case 8: AtDeleteArray(static_cast<AtDataIntArray*>(values)); break;
        case 9: AtDeleteArray(static_cast<AtDataRealArray*>(values)); break;
        case 10: AtDeleteArray(static_cast<AtDataShortNameArray*>(values)); break;
        default: AtDeleteArray(static_cast<AtDataStringArray*>(values)); break;
    }
}

}  // namespace

// ---- a field's description --------------------------------------------------------------------------------------------

int AtDataDescriptor::Read(AtIoTokenizer* tokenizer) {
    if ((m_flags & kTyped) != 0) {
        Clear();
    }
    Expect(tokenizer, kDescriptor);
    while (tokenizer->Next() != AtIoTokenizer::kLeftCurly) {
    }
    for (int word = tokenizer->Next(); word != AtIoTokenizer::kRightCurly; word = tokenizer->Next()) {
        switch (word) {
            case kName: {
                char name[8];
                tokenizer->ExpectName(name);
                AtCopyName8(m_name, name);
                break;
            }
            case kType:
                switch (tokenizer->Next()) {
                    case kBoolean: m_type = 1; break;
                    case kInt: m_type = 2; break;
                    case kReal: m_type = 3; break;
                    case kShortName: m_type = 4; break;
                    case kString: m_type = 5; break;
                    case kBooleanArray: m_type = 6; break;
                    case kByteArray: m_type = 7; break;
                    case kIntArray: m_type = 8; break;
                    case kRealArray: m_type = 9; break;
                    case kShortNameArray: m_type = 10; break;
                    case kStringArray: m_type = 11; break;
                    case kRelational: m_type = kTypeRelational; break;
                    default: tokenizer->Error(AtIoTokenizer::kExpectedKeyword); break;  // Byte among them, though ByteArray is taken
                }
                break;
            case kNumElements:
                m_count = (unsigned int)tokenizer->ExpectInt();
                break;
            case kFloor:
                if (IsReal(m_type)) {
                    m_realFloor = tokenizer->ExpectReal();
                } else {
                    m_intFloor = tokenizer->ExpectInt();
                }
                break;
            case kCeiling:
                if (IsReal(m_type)) {
                    m_realCeiling = tokenizer->ExpectReal();
                } else {
                    m_intCeiling = tokenizer->ExpectInt();
                }
                break;
            case kFormatString:
                tokenizer->ExpectStringCopy(&m_format);
                break;
            default:
                tokenizer->Error(AtIoTokenizer::kExpectedKeyword);  // Flags among them
                break;
        }
    }
    m_flags |= kTyped;
    return 0;
}

// ---- layouts -----------------------------------------------------------------------------------------------------------

void AtRecordLayout::SetHeap(void* heap) {
    s_heap = heap;
}

AtRecordLayout::AtRecordLayout() {
    m_count = 0;
    m_fields = 0;
    m_description = 0;
    m_loaded = 0;
}

void AtRecordLayout::Clear() {
    GmNameMap::Clear();
    if (m_description != 0) {
        operator delete(m_description);
    }
    if (m_fields != 0) {
        operator delete(m_fields);
    }
    m_count = 0;
    m_fields = 0;
    m_description = 0;
    m_loaded = 0;
}

AtRecordLayout::~AtRecordLayout() {
    AtRecordLayout::Clear();
}

AtDataDescriptor* AtRecordLayout::FieldAt(unsigned int index) {
    return m_fields[index];
}

int AtRecordLayout::Read(AtIoTokenizer* tokenizer, GmNameMap* descriptors) {
    if (m_loaded != 0) {
        Clear();
    }
    tokenizer->SetKeywords(AtDatabaseManager::s_keywords, AtDatabaseManager::kKeywordCount, 0);
    Expect(tokenizer, kLayout);
    tokenizer->ExpectLeftCurly();
    Expect(tokenizer, kName);
    char name[8];
    tokenizer->ExpectName(name);
    AtCopyName8(m_name, name);
    Expect(tokenizer, kDesc);
    CopyOnHeap(s_heap, tokenizer->ExpectString(), &m_description);
    Expect(tokenizer, kDataComponents);
    m_count = (unsigned int)tokenizer->ExpectCount();
    Init(m_count);
    m_fields = (AtDataDescriptor**)NewFrom(s_heap, m_count * sizeof(AtDataDescriptor*));
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectName(name);
        m_fields[i] = (AtDataDescriptor*)descriptors->Find(name);
        Insert(name, &m_fields[i]);
    }
    tokenizer->ExpectRightCurly();
    tokenizer->ExpectRightCurly();
    m_loaded = 1;
    return 0;
}

// ---- records -----------------------------------------------------------------------------------------------------------

AtDataRecord::AtDataRecord() {
    m_layout = 0;
    m_inUse = 0;
}

AtDataRecord::~AtDataRecord() {
    m_layout = 0;
    m_inUse = 0;
}

int AtDataRecord::Set(unsigned int index, AtRecordLayout* layout, AtDataComponent** columns) {
    if (m_inUse != 0) {
        m_layout = 0;
        m_inUse = 0;
    }
    m_index = index;
    m_columns = columns;
    m_layout = layout;
    return 0;
}

AtDataComponent* AtDataRecord::FieldAt(unsigned int column) {
    const unsigned int type = m_layout->FieldAt(column)->m_type;
    if (!IsValueType(type)) {
        return 0;
    }
    return (AtDataComponent*)((char*)m_columns[column] + m_index * ValueSize(type));
}

AtDataDescriptor* AtDataRecord::DescriptorAt(unsigned int column) {
    const unsigned int type = m_layout->FieldAt(column)->m_type;
    if (!IsValueType(type)) {
        return 0;
    }
    return ((AtDataComponent*)((char*)m_columns[column] + m_index * ValueSize(type)))->m_descriptor;
}

AtDataComponent* AtDataRecord::FieldNamed(const char* name) {
    const unsigned int count = m_layout->m_count;
    unsigned int column = 0;
    for (; column < count; column++) {
        char own[8];
        AtCopyName8(own, m_layout->FieldAt(column)->m_name);
        if (strncmp(own, name, 8) == 0) {
            break;
        }
    }
    return FieldAt(column);
}

// ---- tables of records -------------------------------------------------------------------------------------------------

void AtDataRecordDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

void AtDataRecordDatabase::Reset() {
    m_count = 0;
    m_unknown24 = 0;
    m_columns = 0;
    m_unknown2C = 0;
    m_characters = 0;
    m_characterSize = 0;
    m_characterUsed = 0;
}

AtDataRecordDatabase::AtDataRecordDatabase() {
    m_count = 0;
    m_unknown24 = 0;
    m_columns = 0;
    m_unknown2C = 0;
    m_characters = 0;
    m_characterSize = 0;
    m_characterUsed = 0;
}

void AtDataRecordDatabase::Clear() {
    const unsigned int columns = m_layout->m_count;
    for (unsigned int column = 0; column < columns; column++) {
        AtDataComponent* values = m_columns[column];
        if (values != 0) {
            if (values->m_descriptor->m_type == kTypeString) {
                char* value = (char*)values;
                for (unsigned int i = 0; i < m_count; i++) {
                    AtDataStringReset((AtDataString*)value);
                    value += ValueSize(kTypeString);
                }
            }
            DeleteColumn(m_layout->FieldAt(column)->m_type, m_columns[column]);
        }
    }
    if (m_columns != 0) {
        operator delete(m_columns);
    }
    delete[] m_records;
    if (m_description != 0) {
        operator delete(m_description);
    }
    if (m_characters != 0) {
        operator delete(m_characters);
    }
    Reset();
}

AtDataRecordDatabase::~AtDataRecordDatabase() {
    AtDataRecordDatabase::Clear();
}

int AtDataRecordDatabase::Load(AtDatabaseManager* manager, AtRecordLayout* layout, AtIoTokenizer* tokenizer, unsigned int extra, int unused, int fromFile) {
    m_layout = layout;
    return Read(manager, tokenizer, extra, unused, fromFile);
}

AtDataRecord* AtDataRecordDatabase::RecordAt(unsigned int index) {
    return &m_records[index];
}

AtDataRecord* AtDataRecordDatabase::FindByValue(unsigned int column, const void* value) {
    for (unsigned int i = 0; i < m_count; i++) {
        if (RecordAt(i)->FieldAt(column)->Equals((const AtDataComponent*)value) != 0) {
            return RecordAt(i);
        }
    }
    return 0;
}

int AtDataRecordDatabase::Unknown7(int) {
    return 0;
}

AtDataRecord* AtDataRecordDatabase::TakeUnused(unsigned int* index) {
    for (unsigned int i = 0; i < m_count; i++) {
        AtDataRecord* record = RecordAt(i);
        if (record->m_inUse == 0) {
            *index = i;
            record->m_inUse = 1;
            return record;
        }
    }
    return 0;
}

void AtDataRecordDatabase::OnReady() {
}

int AtDataRecordDatabase::Read(AtDatabaseManager* manager, AtIoTokenizer* tokenizer, unsigned int extra, int, int fromFile) {
    if (m_columns != 0) {
        Clear();
    }
    Expect(tokenizer, kName);
    char name[8];
    tokenizer->ExpectName(name);
    AtCopyName8(m_name, name);
    Expect(tokenizer, kDesc);
    CopyOnHeap(s_heap, tokenizer->ExpectString(), &m_description);
    Expect(tokenizer, kRecordLayout);
    tokenizer->ExpectName(name);
    m_layout = (AtRecordLayout*)manager->Find(name);
    Expect(tokenizer, kDataRecords);
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (fromFile == 0) {
        m_count = 0;
    }
    m_count += extra;

    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_records = new AtDataRecord[m_count];
    AtPopHeap();
    if (m_records == 0) {
        AtRaise(-4, 0, 0, 0);
    }

    const unsigned int columns = m_layout->m_count;
    m_characterSize = 0;
    for (unsigned int column = 0; column < columns; column++) {
        const AtDataDescriptor* field = m_layout->FieldAt(column);
        if (field->m_type == kTypeString) {
            m_characterSize += (field->m_intCeiling + 1) * manager->m_characterScale;
        } else if (field->m_type == kTypeStringArray) {
            m_characterSize += (field->m_intCeiling + 1) * field->m_count * manager->m_characterScale;
        }
    }
    if (m_characterSize != 0) {
        m_characterSize *= m_count;
        m_characters = (char*)NewFrom(s_heap, m_characterSize);
    }

    m_columns = (AtDataComponent**)NewFrom(s_heap, columns * sizeof(AtDataComponent*));
    for (unsigned int column = 0; column < columns; column++) {
        const unsigned int type = m_layout->FieldAt(column)->m_type;
        if (IsValueType(type)) {
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_columns[column] = NewColumn(type, m_count);
            AtPopHeap();
            if (m_columns[column] == 0) {
                AtRaise(-4, 0, 0, 0);
            }
        }
    }

    for (unsigned int i = 0; i < m_count; i++) {
        AtDataRecord& record = m_records[i];
        record.Set(i, m_layout, m_columns);
        const int given = i < m_count - extra ? 1 : 0;
        record.m_inUse = given;
        for (unsigned int column = 0; column < columns; column++) {
            AtDataDescriptor* field = m_layout->FieldAt(column);
            const unsigned int type = field->m_type;
            if (!IsValueType(type)) {
                AtFatal("AtDataRecordDatabase: a field of no type a table can hold; the original crashes here", 0, 0, 0);
                continue;
            }
            AtDataComponent* value = (AtDataComponent*)((char*)m_columns[column] + i * ValueSize(type));
            if (type == kTypeString) {
                char* characters = m_characters + m_characterUsed;
                const unsigned int size = field->m_intCeiling + 1;
                m_characterUsed += size * manager->m_characterScale;
                if (given != 0) {
                    AtDataStringBindAndRead((AtDataString*)value, field, tokenizer, characters, size);
                } else {
                    AtDataStringBind((AtDataString*)value, field, characters, size);
                }
            } else if (given != 0) {
                value->Read(field, tokenizer);
            } else {
                value->SetDescriptor(field);
            }
        }
    }

    if (fromFile != 0) {
        tokenizer->ExpectRightCurly();
        tokenizer->ExpectRightCurly();
    }
    return 0;
}

// ---- the manager -------------------------------------------------------------------------------------------------------

void AtDatabaseManager::SetHeap(void* heap) {
    s_heap = heap;
}

AtDatabaseManager::AtDatabaseManager() {
    m_descriptors = 0;
    m_layouts = 0;
    m_tables = 0;
    m_loaded = 0;
    s_first = 0;
    m_next = 0;
    m_fileName = 0;
    m_characterScale = 1;
    if (s_first != 0) {
        AtDatabaseManager* last = s_first;
        while (last->m_next != 0) {
            last = last->m_next;
        }
        last->m_next = this;
    } else {
        s_first = this;
    }
}

void AtDatabaseManager::Clear() {
    GmNameMap::Clear();
    m_descriptorsByName.Clear();
    if (s_first != 0) {
        if (s_first == this) {
            s_first = m_next;
        } else {
            // Stops at the last if this is not on the list, which then takes this one's next.
            AtDatabaseManager* before = s_first;
            AtDatabaseManager* at = before->m_next;
            while (at != this && at != 0) {
                before = at;
                at = at->m_next;
            }
            before->m_next = m_next;
        }
    }
    for (unsigned int i = 0; i < m_tableCount; i++) {
        if (m_tables != 0 && m_tables[i] != 0) {
            delete m_tables[i];
        }
    }
    if (m_tables != 0) {
        operator delete(m_tables);
    }
    delete[] m_layouts;
    if (m_descriptors != 0) {
        AtDeleteArray(m_descriptors);
    }
    if (m_fileName != 0) {
        operator delete(m_fileName);
    }
    m_descriptors = 0;
    m_layouts = 0;
    m_tables = 0;
    m_loaded = 0;
    s_first = 0;
    m_next = 0;
    m_fileName = 0;
    m_characterScale = 1;
}

AtDatabaseManager::~AtDatabaseManager() {
    AtDatabaseManager::Clear();
}

void* AtDatabaseManager::FindEverywhere(const char* name) {
    void* found = Find(name);
    if (found == 0) {
        for (AtDatabaseManager* manager = s_first; manager != 0; manager = manager->m_next) {
            found = manager->Find(name);
            if (found != 0) {
                break;
            }
        }
    }
    return found;
}

int AtDatabaseManager::Load(AtDataRegistryOwner* owner, const char* name, int, char* spaceOut, unsigned int extra, int fromFile) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ADB", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ADB");
    }
    CopyOnHeap(s_heap, file.m_file.m_file->m_path, &m_fileName);
    Read(owner, file.m_tokenizer, spaceOut, extra, fromFile);
    file.Close();
    m_loaded = 1;
    return 0;
}

int AtDatabaseManager::Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer, char* spaceOut, unsigned int extra, int fromFile) {
    if (m_loaded != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char name[8];
    int word = tokenizer->Next();
    if (word == kNameSpace) {
        char space[8];
        char* into = spaceOut != 0 ? spaceOut : space;
        tokenizer->ExpectName(into);
        owner->AddDatabase(this, into);
        word = tokenizer->Next();
    }
    if (word != kDescriptors) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }

    m_descriptorCount = (unsigned int)tokenizer->ExpectCount();
    if (m_descriptorCount != 0) {
        m_descriptorsByName.Init(m_descriptorCount);
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_descriptors = AtNewArray<AtDataDescriptor>(m_descriptorCount);
        AtPopHeap();
        if (m_descriptors == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_descriptorCount; i++) {
            m_descriptors[i].Read(tokenizer);
            AtCopyName8(name, m_descriptors[i].m_name);
            m_descriptorsByName.Insert(name, &m_descriptors[i]);
        }
    }
    tokenizer->ExpectRightCurly();

    Expect(tokenizer, kLayouts);
    m_layoutCount = (unsigned int)tokenizer->ExpectCount();
    if (m_layoutCount != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_layouts = new AtRecordLayout[m_layoutCount];
        AtPopHeap();
        if (m_layouts == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_layoutCount; i++) {
            m_layouts[i].Read(tokenizer, &m_descriptorsByName);
        }
    }
    tokenizer->ExpectRightCurly();

    Expect(tokenizer, kTables);
    m_tableCount = (unsigned int)tokenizer->ExpectCount();
    if (m_tableCount != 0) {
        Init(m_layoutCount + m_tableCount);
        for (unsigned int i = 0; i < m_layoutCount; i++) {
            AtCopyName8(name, m_layouts[i].m_name);
            Insert(name, &m_layouts[i]);
        }
        m_tables = (AtDataRecordDatabase**)NewFrom(s_heap, m_tableCount * sizeof(AtDataRecordDatabase*));
        for (unsigned int i = 0; i < m_tableCount; i++) {
            tokenizer->ExpectToken(kTable);
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_tables[i] = new AtDataRecordDatabase;
            AtPopHeap();
            if (m_tables[i] == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            tokenizer->ExpectLeftCurly();
            m_tables[i]->Read(this, tokenizer, extra, 0, fromFile);
            AtCopyName8(name, m_tables[i]->m_name);
            Insert(name, m_tables[i]);
        }
    }
    if (fromFile != 0) {
        tokenizer->ExpectRightCurly();
    }
    for (unsigned int i = 0; i < m_tableCount; i++) {
        m_tables[i]->OnReady();
    }
    m_loaded = 1;
    return 0;
}
