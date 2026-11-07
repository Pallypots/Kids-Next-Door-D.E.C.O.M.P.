// See include/at/AtInputMapper.h.
#include "at/AtInputMapper.h"

#include "AtSceneArray.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtInputMapper::s_heap;
void* AtInputMapperDatabase::s_heap;

namespace {

enum Word {
    kNameSpace = 0x39, kAbstractMapper = 0x3A, kButtonMap = 0x3B, kAxisMap = 0x3C, kPhysicalID = 0x3D, kPhysicalPOVAxis = 0x3E, kAbstractID = 0x3F, kAbstractPOVAxis = 0x40, kPhysicalButtonState = 0x41,
    kAbstractAxisState = 0x42, kAbstractPOVState = 0x43, kButtonUp = 0x44, kButtonDown = 0x45, kAxisPositive = 0x46, kAxisNegative = 0x47, kPOVUp = 0x48, kPOVUpRight = 0x49, kPOVRight = 0x4A,
    kPOVDownRight = 0x4B, kPOVDown = 0x4C, kPOVDownLeft = 0x4D, kPOVLeft = 0x4E, kPOVUpLeft = 0x4F
};

// Input codes have these high parts, and every word below the first is not one.
const int kFirstInputCode = 0x10000000;
const int kFirstAxisCode = 0x40000000;

const int kNegative = 0x10000;
const int kNoPOVState = kPOVUpLeft + 1 - kPOVUp;

// The original has this inline at each use. Not cleared.
template <class T> T* NewMaps(unsigned int count) {
    if (AtInputMapper::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtInputMapper::s_heap);
    T* maps = (T*)operator new(count * sizeof(T));
    AtPopHeap();
    if (maps == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return maps;
}

template <class T> T* NewClearedMaps(unsigned int count) {
    T* maps = NewMaps<T>(count);
    unsigned char* bytes = (unsigned char*)maps;
    for (unsigned int i = 0; i < count * sizeof(T); i++) {
        bytes[i] = 0;
    }
    return maps;
}

}  // namespace

const AtIoKeyword AtInputMapperDatabase::s_keywords[AtInputMapperDatabase::kKeywordCount] = {
    {kNameSpace, "NameSpace"},
    {kAbstractMapper, "AbstractMapper"},
    {kButtonMap, "ButtonMap"},
    {kAxisMap, "AxisMap"},
    {kPhysicalID, "PhysicalID"},
    {kPhysicalPOVAxis, "PhysicalPOVAxis"},
    {kAbstractID, "AbstractID"},
    {kAbstractPOVAxis, "AbstractPOVAxis"},
    {kPhysicalButtonState, "PhysicalButtonState"},
    {kAbstractAxisState, "AbstractAxisState"},
    {kAbstractPOVState, "AbstractPOVState"},
    {kButtonUp, "ButtonUp"},
    {kButtonDown, "ButtonDown"},
    {kAxisPositive, "AxisPositive"},
    {kAxisNegative, "AxisNegative"},
    {kPOVUp, "POVUp"},
    {kPOVUpRight, "POVUpRight"},
    {kPOVRight, "POVRight"},
    {kPOVDownRight, "POVDownRight"},
    {kPOVDown, "POVDown"},
    {kPOVDownLeft, "POVDownLeft"},
    {kPOVLeft, "POVLeft"},
    {kPOVUpLeft, "POVUpLeft"},
};

// In the original's order. Two codes have two names each.
const AtIoKeyword AtInputMapperDatabase::s_inputCodes[AtInputMapperDatabase::kInputCodeCount] = {
    {0x10000001, "KeyEscape"},
    {0x10000002, "Key1"},
    {0x10000003, "Key2"},
    {0x10000004, "Key3"},
    {0x10000005, "Key4"},
    {0x10000006, "Key5"},
    {0x10000007, "Key6"},
    {0x10000008, "Key7"},
    {0x10000009, "Key8"},
    {0x1000000A, "Key9"},
    {0x1000000B, "Key0"},
    {0x1000000C, "KeyMinus"},
    {0x1000000D, "KeyEquals"},
    {0x1000000E, "KeyBack"},
    {0x1000000F, "KeyTab"},
    {0x10000010, "KeyQ"},
    {0x10000011, "KeyW"},
    {0x10000012, "KeyE"},
    {0x10000013, "KeyR"},
    {0x10000014, "KeyT"},
    {0x10000015, "KeyY"},
    {0x10000016, "KeyU"},
    {0x10000017, "KeyI"},
    {0x10000018, "KeyO"},
    {0x10000019, "KeyP"},
    {0x1000001A, "KeyLeftBracket"},
    {0x1000001B, "KeyRightBracket"},
    {0x1000001C, "KeyReturn"},
    {0x1000001D, "KeyLeftControl"},
    {0x1000001E, "KeyA"},
    {0x1000001F, "KeyS"},
    {0x10000020, "KeyD"},
    {0x10000021, "KeyF"},
    {0x10000022, "KeyG"},
    {0x10000023, "KeyH"},
    {0x10000024, "KeyJ"},
    {0x10000025, "KeyK"},
    {0x10000026, "KeyL"},
    {0x10000027, "KeySemicolon"},
    {0x10000028, "KeyApostrophe"},
    {0x10000029, "KeyGrave"},
    {0x1000002A, "KeyLeftShift"},
    {0x1000002B, "KeyBackslash"},
    {0x1000002C, "KeyZ"},
    {0x1000002D, "KeyX"},
    {0x1000002E, "KeyC"},
    {0x1000002F, "KeyV"},
    {0x10000030, "KeyB"},
    {0x10000031, "KeyN"},
    {0x10000032, "KeyM"},
    {0x10000033, "KeyComma"},
    {0x10000034, "KeyPeriod"},
    {0x10000035, "KeySlash"},
    {0x10000036, "KeyRightShift"},
    {0x10000037, "KeyMultiply"},
    {0x10000038, "KeyLeftAlt"},
    {0x10000039, "KeySpace"},
    {0x1000003A, "KeyCapital"},
    {0x1000003B, "KeyF1"},
    {0x1000003C, "KeyF2"},
    {0x1000003D, "KeyF3"},
    {0x1000003E, "KeyF4"},
    {0x1000003F, "KeyF5"},
    {0x10000040, "KeyF6"},
    {0x10000041, "KeyF7"},
    {0x10000042, "KeyF8"},
    {0x10000043, "KeyF9"},
    {0x10000044, "KeyF10"},
    {0x10000045, "KeyNumlock"},
    {0x10000046, "KeyScrollLock"},
    {0x10000047, "KeyNumPad7"},
    {0x10000048, "KeyNumPad8"},
    {0x10000049, "KeyNumPad9"},
    {0x1000004A, "KeySubtract"},
    {0x1000004B, "KeyNumPad4"},
    {0x1000004C, "KeyNumPad5"},
    {0x1000004D, "KeyNumPad6"},
    {0x1000004E, "KeyNumPadAdd"},
    {0x1000004F, "KeyNumPad1"},
    {0x10000050, "KeyNumPad2"},
    {0x10000051, "KeyNumPad3"},
    {0x10000052, "KeyNumPad0"},
    {0x10000053, "KeyDecimal"},
    {0x10000056, "KeyOEM_102"},
    {0x10000057, "KeyF11"},
    {0x10000058, "KeyF12"},
    {0x10000064, "KeyF13"},
    {0x10000065, "KeyF14"},
    {0x10000066, "KeyF15"},
    {0x10000070, "KeyKana"},
    {0x10000073, "KeyAbnt_c1"},
    {0x10000079, "KeyConvert"},
    {0x1000007B, "KeyNoConvert"},
    {0x1000007D, "KeyYen"},
    {0x1000007E, "KeyAbnt_c2"},
    {0x1000008D, "KeyNumPadEquals"},
    {0x10000090, "KeyPrevTrack"},
    {0x10000091, "KeyAT"},
    {0x10000092, "KeyColon"},
    {0x10000093, "KeyUnderline"},
    {0x10000094, "KeyKanji"},
    {0x10000095, "KeyStop"},
    {0x10000096, "KeyAX"},
    {0x10000097, "KeyUnlabeled"},
    {0x10000099, "KeyNextTrack"},
    {0x1000009C, "KeyNumPadEnter"},
    {0x1000009D, "KeyRightControl"},
    {0x100000A0, "KeyMute"},
    {0x100000A1, "KeyCalculator"},
    {0x100000A2, "KeyPlayPause"},
    {0x100000A4, "KeyMediaStop"},
    {0x100000AE, "KeyVolumeDown"},
    {0x100000B0, "KeyVolumeUp"},
    {0x100000B2, "KeyWebHome"},
    {0x100000B3, "KeyNumPadComma"},
    {0x100000B5, "KeyDivide"},
    {0x100000B7, "KeySystemRequest"},
    {0x100000B8, "KeyRightAlt"},
    {0x100000C5, "KeyPause"},
    {0x100000C7, "KeyHome"},
    {0x100000C8, "KeyUp"},
    {0x100000C9, "KeyPrior"},
    {0x100000CB, "KeyLeft"},
    {0x100000CD, "KeyRight"},
    {0x100000CF, "KeyEnd"},
    {0x100000D0, "KeyDown"},
    {0x100000D1, "KeyNext"},
    {0x100000D2, "KeyInsert"},
    {0x100000D3, "KeyDelete"},
    {0x100000DB, "KeyLeftWin"},
    {0x100000DC, "KeyRightWin"},
    {0x100000DD, "KeyApps"},
    {0x100000DE, "KeyPower"},
    {0x100000DF, "KeySleep"},
    {0x100000E3, "KeyWake"},
    {0x100000E5, "KeyWebSearch"},
    {0x100000E6, "KeyWebFavorites"},
    {0x100000E7, "KeyWebRefresh"},
    {0x100000E8, "KeyWebStop"},
    {0x100000E9, "KeyWebForward"},
    {0x100000EA, "KeyWebBack"},
    {0x100000EB, "KeyMyComputer"},
    {0x100000EC, "KeyMail"},
    {0x100000ED, "KeyMediaSelect"},
    {0x20000003, "MouseLeftButton"},
    {0x20000004, "MouseRightButton"},
    {0x20000005, "MouseMiddleButton"},
    {0x20000005, "MouseChordButton"},
    {0x20000006, "MouseButton4"},
    {0x20000007, "MouseButton5"},
    {0x20000008, "MouseButton6"},
    {0x20000009, "MouseButton7"},
    {0x2000000A, "MouseButton8"},
    {0x30000000, "JoystickButton0"},
    {0x30000001, "JoystickButton1"},
    {0x30000002, "JoystickButton2"},
    {0x30000003, "JoystickButton3"},
    {0x30000004, "JoystickButton4"},
    {0x30000005, "JoystickButton5"},
    {0x30000006, "JoystickButton6"},
    {0x30000007, "JoystickButton7"},
    {0x30000008, "JoystickButton8"},
    {0x30000009, "JoystickButton9"},
    {0x3000000A, "JoystickButton10"},
    {0x3000000B, "JoystickButton11"},
    {0x3000000C, "JoystickButton12"},
    {0x3000000D, "JoystickButton13"},
    {0x3000000E, "JoystickButton14"},
    {0x3000000F, "JoystickButton15"},
    {0x30000010, "JoystickButton16"},
    {0x30000011, "JoystickButton17"},
    {0x30000012, "JoystickButton18"},
    {0x30000013, "JoystickButton19"},
    {0x30000014, "JoystickButton20"},
    {0x30000015, "JoystickButton21"},
    {0x30000016, "JoystickButton22"},
    {0x30000017, "JoystickButton23"},
    {0x30000018, "JoystickButton24"},
    {0x30000019, "JoystickButton25"},
    {0x3000001A, "JoystickButton26"},
    {0x3000001B, "JoystickButton27"},
    {0x3000001C, "JoystickButton28"},
    {0x3000001D, "JoystickButton29"},
    {0x3000001E, "JoystickButton30"},
    {0x3000001F, "JoystickButton31"},
    {0x30000020, "JoystickButton32"},
    {0x30000021, "JoystickButton33"},
    {0x30000022, "JoystickButton34"},
    {0x30000023, "JoystickButton35"},
    {0x30000024, "JoystickButton36"},
    {0x30000025, "JoystickButton37"},
    {0x30000026, "JoystickButton38"},
    {0x30000027, "JoystickButton39"},
    {0x30000028, "JoystickButton40"},
    {0x30000029, "JoystickButton41"},
    {0x3000002A, "JoystickButton42"},
    {0x3000002B, "JoystickButton43"},
    {0x3000002C, "JoystickButton44"},
    {0x3000002D, "JoystickButton45"},
    {0x3000002E, "JoystickButton46"},
    {0x3000002F, "JoystickButton47"},
    {0x30000030, "JoystickButton48"},
    {0x30000031, "JoystickButton49"},
    {0x30000032, "JoystickButton50"},
    {0x30000033, "JoystickButton51"},
    {0x30000034, "JoystickButton52"},
    {0x30000035, "JoystickButton53"},
    {0x30000036, "JoystickButton54"},
    {0x30000037, "JoystickButton55"},
    {0x30000038, "JoystickButton56"},
    {0x30000039, "JoystickButton57"},
    {0x3000003A, "JoystickButton58"},
    {0x3000003B, "JoystickButton59"},
    {0x3000003C, "JoystickButton60"},
    {0x3000003D, "JoystickButton61"},
    {0x3000003E, "JoystickButton62"},
    {0x3000003F, "JoystickButton63"},
    {0x40000000, "AxisX"},
    {0x40000001, "AxisY"},
    {0x40000002, "AxisZ"},
    {0x40000003, "AxisU"},
    {0x40000004, "AxisV"},
    {0x40000005, "AxisRx"},
    {0x40000006, "AxisRy"},
    {0x40000007, "AxisRz"},
    {0x40000008, "AxisLx"},
    {0x40000009, "AxisLy"},
    {0x4000000A, "AxisLz"},
    {0x4000000B, "AxisSlider"},
    {0x4000000B, "AxisSliderX"},
    {0x4000000C, "AxisSliderY"},
    {0x4000000D, "Axis13"},
    {0x4000000E, "Axis14"},
    {0x4000000F, "Axis15"},
    {0x50000000, "POV"},
};

// ---- one mapper -----------------------------------------------------------------------------------------------------

void AtInputMapper::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputMapper::AtInputMapper() {
    m_device = 0;
    m_buttonMaps = 0;
    m_axisMaps = 0;
    m_buttonMapCapacity = 0;
    m_buttonMapsUsed = 0;
    m_axisMapCapacity = 0;
    m_axisMapsUsed = 0;
}

AtInputMapper::~AtInputMapper() {
    Clear();
}

void AtInputMapper::Clear() {
    if (m_device == 0) {
        return;
    }
    m_router.Release();
    if (m_buttonMaps != 0) {
        operator delete(m_buttonMaps);
    }
    if (m_axisMapCapacity != 0) {
        operator delete(m_axisMaps);
    }
    m_device = 0;
    m_buttonMaps = 0;
    m_axisMaps = 0;
    m_buttonMapCapacity = 0;
    m_buttonMapsUsed = 0;
    m_axisMapCapacity = 0;
    m_axisMapsUsed = 0;
}

void AtInputMapper::ReadButtonMaps(AtIoTokenizer* tokenizer) {
    if (m_buttonMapCapacity == 0) {
        return;
    }
    m_buttonMaps = NewMaps<AtInputButtonMap>(m_buttonMapCapacity);
    for (unsigned int i = 0; i < m_buttonMapCapacity >> 1; i++) {
        tokenizer->ExpectToken(kButtonMap);
        tokenizer->ExpectLeftCurly();
        int physical = 0;
        int abstractId = 0;
        int state = 1;
        int negative = 0;
        int povState = kNoPOVState;
        int token = tokenizer->Next();
        if (token == AtIoTokenizer::kRightCurly) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        } else {
            do {
                switch (token) {
                case kPhysicalID:
                    physical = tokenizer->Next();
                    if (physical < kFirstInputCode) {
                        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    }
                    break;
                case kAbstractID:
                    abstractId = tokenizer->Next();
                    if (abstractId < kFirstInputCode) {
                        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    }
                    break;
                case kPhysicalButtonState: {
                    const int word = tokenizer->Next();
                    if (word == kAxisPositive) {
                        state = 2;
                    } else if (word == kAxisNegative) {
                        state = 3;
                    } else if (word >= kPOVUp && word <= kPOVUpLeft) {
                        state = word + 0xFB8;
                    } else {
                        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    }
                    break;
                }
                case kAbstractAxisState: {
                    const int word = tokenizer->Next();
                    if (word == kAxisNegative) {
                        negative = 1;
                    } else if (word != kAxisPositive) {
                        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    }
                    break;
                }
                case kAbstractPOVState:
                    povState = tokenizer->Next() - kPOVUp;
                    break;
                default:
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    break;
                }
                token = tokenizer->Next();
            } while (token != AtIoTokenizer::kRightCurly);
            if (physical == 0 || abstractId == 0) {
                tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            }
        }
        if (negative != 0) {
            abstractId |= kNegative;
        }
        // The original's: the token, which is always the closing brace here, not the state read for it. So any POV state gives the same bits, and which one is lost.
        if (povState != kNoPOVState) {
            abstractId |= token << 16;
        }
        m_buttonMaps[m_buttonMapsUsed].m_physical = physical;
        m_buttonMaps[m_buttonMapsUsed].m_abstract = abstractId;
        m_buttonMaps[m_buttonMapsUsed].m_state = state;
        m_buttonMapsUsed++;
        m_buttonMaps[m_buttonMapsUsed].m_physical = physical;
        m_buttonMaps[m_buttonMapsUsed].m_abstract = abstractId;
        m_buttonMaps[m_buttonMapsUsed].m_state = 0;
        m_buttonMapsUsed++;
    }
    tokenizer->ExpectRightCurly();
}

void AtInputMapper::ReadAxisMaps(AtIoTokenizer* tokenizer) {
    if (m_axisMapCapacity == 0) {
        return;
    }
    m_axisMaps = NewMaps<AtInputAxisMap>(m_axisMapCapacity);
    for (unsigned int i = 0; i < m_axisMapCapacity; i++) {
        tokenizer->ExpectToken(kAxisMap);
        tokenizer->ExpectLeftCurly();
        int physical = 0;
        int abstractId = 0;
        int negative = 0;
        int token = tokenizer->Next();
        while (token != AtIoTokenizer::kRightCurly) {
            switch (token) {
            case kPhysicalID:
                physical = tokenizer->Next();
                if (physical < kFirstInputCode) {
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                }
                break;
            case kAbstractID:
                abstractId = tokenizer->Next();
                if (abstractId < kFirstInputCode) {
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                }
                break;
            case kPhysicalPOVAxis:
            case kAbstractPOVAxis: {
                const int word = tokenizer->Next();
                if (word == kFirstAxisCode + 1) {
                    negative = 1;
                } else if (word != kFirstAxisCode) {
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                }
                break;
            }
            default:
                tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                break;
            }
            token = tokenizer->Next();
        }
        if (negative != 0) {
            abstractId |= kNegative;
        }
        m_axisMaps[m_axisMapsUsed].m_physical = physical;
        m_axisMaps[m_axisMapsUsed].m_abstract = abstractId;
        m_axisMapsUsed++;
    }
    tokenizer->ExpectRightCurly();
}

void AtInputMapper::Allocate(const Sizes& sizes) {
    Clear();
    m_device = sizes.m_device;
    m_buttonMapCapacity = sizes.m_buttonMaps * 2;
    m_axisMapCapacity = sizes.m_axisMaps;
    m_router.Allocate(m_device, m_buttonMapCapacity, m_axisMapCapacity);
    if (m_buttonMapCapacity != 0) {
        m_buttonMaps = NewClearedMaps<AtInputButtonMap>(m_buttonMapCapacity);
    }
    if (m_axisMapCapacity != 0) {
        m_axisMaps = NewClearedMaps<AtInputAxisMap>(m_axisMapCapacity);
    }
}

void AtInputMapper::CopyFrom(const AtInputMapper& from) {
    Sizes sizes;
    sizes.m_device = from.m_device;
    sizes.m_buttonMaps = from.m_buttonMapCapacity;
    sizes.m_axisMaps = from.m_axisMapCapacity;
    Allocate(sizes);
    if (m_buttonMapCapacity != 0) {
        m_buttonMapsUsed = from.m_buttonMapsUsed;
        for (unsigned int i = 0; i < m_buttonMapsUsed; i++) {
            m_buttonMaps[i] = from.m_buttonMaps[i];
        }
    }
    if (m_axisMapCapacity != 0) {
        m_axisMapsUsed = from.m_axisMapsUsed;
        for (unsigned int i = 0; i < m_axisMapsUsed; i++) {
            m_axisMaps[i] = from.m_axisMaps[i];
        }
    }
}

void AtInputMapper::Read(AtInputOwner* owner, AtIoTokenizer* tokenizer) {
    Clear();
    m_device = owner->m_device;
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kAbstractMapper: {
            // A copy of a mapper read before. Nothing may follow it.
            char name[8];
            char space[8];
            tokenizer->ExpectName(name);
            tokenizer->ExpectName(space);
            const AtInputMapper* from = (const AtInputMapper*)(space[0] != 0 ? owner->m_mapperSpaces.Find(space, name) : owner->m_mappers.Find(name));
            CopyFrom(*from);
            tokenizer->ExpectRightCurly();
            return;
        }
        case kButtonMap:
            m_buttonMapCapacity = (unsigned int)tokenizer->ExpectCount() * 2;
            ReadButtonMaps(tokenizer);
            break;
        case kAxisMap:
            m_axisMapCapacity = (unsigned int)tokenizer->ExpectCount();
            ReadAxisMaps(tokenizer);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    m_router.Allocate(m_device, m_buttonMapCapacity, m_axisMapCapacity);
}

// ---- the owner --------------------------------------------------------------------------------------------------------

void AtInputOwner::AddMappers(GmNameMap* map, const char* ns) {
    if (ns == 0) {
        m_mappers.Add(map);
    } else {
        m_mapperSpaces.Add(ns, map);
    }
}

// ---- the database -------------------------------------------------------------------------------------------------------

void AtInputMapperDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputMapperDatabase::AtInputMapperDatabase() {
    m_owner = 0;
    m_items = 0;
    m_unknown1C = 0;
}

AtInputMapperDatabase::~AtInputMapperDatabase() {
    if (m_items != 0) {
        AtDeleteArray(m_items);
        m_owner = 0;
        m_items = 0;
        m_unknown1C = 0;
        GmNameMap::Clear();
    }
}

void AtInputMapperDatabase::Clear() {
    if (m_items != 0) {
        AtDeleteArray(m_items);
        m_owner = 0;
        m_items = 0;
        m_unknown1C = 0;
        GmNameMap::Clear();
    }
}

void AtInputMapperDatabase::Read(AtInputOwner* owner, AtIoTokenizer* tokenizer) {
    AtIoTextTokenizer codes;
    const char* ns = 0;
    Clear();
    m_owner = owner;
    codes.SetKeywords(s_inputCodes, kInputCodeCount, 0);
    tokenizer->SetKeywords(s_keywords, kKeywordCount, &codes);
    char space[8];
    int token = tokenizer->Next();
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    if (token != kAbstractMapper) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const unsigned int count = (unsigned int)tokenizer->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_items = AtNewArray<AtInputMapper>(count);
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(count);
    owner->AddMappers(this, ns);
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->ExpectToken(kAbstractMapper);
        char name[8];
        tokenizer->ExpectName(name);
        m_items[i].Read(m_owner, tokenizer);
        Insert(name, &m_items[i]);
    }
    tokenizer->ExpectRightCurly();
}

void AtInputMapperDatabase::Load(AtInputOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ANM", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ANM");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
