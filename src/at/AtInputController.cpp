// See include/at/AtInputController.h.
#include "at/AtInputController.h"

#include "AtArray.h"
#include "at/AtInputMapper.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtInputEventQueue::s_heap;
void* AtInputController::s_heap;
void* AtInputControllerDatabase::s_heap;
float AtAbstractController::s_povAngles[AtAbstractController::kPovDirections];

namespace {

enum {
    kNameSpace = 0x39,
    kAbstractController = 0x3A,
    kButtons = 0x3B,
    kAxes = 0x3C,
    kPovs = 0x3D,
    kEventHandlers = 0x3E,
    kDeadZone = 0x3F,
    kSaturation = 0x40,
};

const float kTwoPi = 6.2831855f;   // 0x40C90FDB
const float kQuietBand = 5.0f;     // 0x00279624 around the center, nothing at all
const float kPercent = 0.01f;      // 0x002719C8
const float kOne = 1.0f;           // 0x00272AC4
const float kMinusOne = -1.0f;     // 0x00279620

void* NewFromControllerHeap(unsigned int bytes) {
    if (AtInputController::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtInputController::s_heap);
    void* block = operator new(bytes);
    AtPopHeap();
    return block;
}

void Require(void* block) {
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void ZeroInts(int* to, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        to[i] = 0;
    }
}

AtInputController::Chain* NewChains(unsigned int count) {
    AtInputController::Chain* chains = (AtInputController::Chain*)NewFromControllerHeap(count * sizeof(AtInputController::Chain));
    if (chains != 0) {
        for (unsigned int i = 0; i < count; i++) {
            chains[i].m_first = 0;
            chains[i].m_last = 0;
        }
    }
    return chains;
}

void Free(void* block) {
    if (block != 0) {
        operator delete(block);
    }
}

}  // namespace

// ---- a handler -------------------------------------------------------------------------------------------------------------

AtInputAxisHandler::AtInputAxisHandler() {
    m_mode = 0;
    m_raw = 0;
    m_id = 0;
    m_index = 0;
    m_minimum = 0.0f;
    m_maximum = 0.0f;
    m_center = 0.0f;
    m_deadZone = 0.0f;
    m_saturation = 100.0f;
    m_value = 0.0f;
    m_previousValue = 0.0f;
    m_filtered = 0.0f;
    m_previousFiltered = 0.0f;
    m_isSet = 0;
    m_flags = 0;
}

AtInputAxisHandler::~AtInputAxisHandler() {
    if (m_isSet != 0) {
        m_mode = 0;
        m_raw = 0;
        m_id = 0;
        m_index = 0;
        m_minimum = 0.0f;
        m_maximum = 0.0f;
        m_center = 0.0f;
        m_deadZone = 0.0f;
        m_saturation = 100.0f;
        m_value = 0.0f;
        m_previousValue = 0.0f;
        m_filtered = 0.0f;
        m_previousFiltered = 0.0f;
        m_isSet = 0;
        m_flags = 0;
    }
}

void AtInputAxisHandler::Reset() {
    if (m_isSet != 0) {
        m_mode = 0;
        m_raw = 0;
        m_id = 0;
        m_index = 0;
        m_minimum = 0.0f;
        m_maximum = 0.0f;
        m_center = 0.0f;
        m_deadZone = 0.0f;
        m_saturation = 100.0f;
        m_value = 0.0f;
        m_previousValue = 0.0f;
        m_filtered = 0.0f;
        m_previousFiltered = 0.0f;
        m_isSet = 0;
        m_flags = 0;
    }
}

void AtInputAxisHandler::SetRaw(int raw) {
    m_raw = raw;
}

void AtInputAxisHandler::SetMode(int mode) {
    m_mode = mode;
}

float AtInputAxisHandler::SetMinimum(float value) {
    m_minimum = value;
    return value;
}

float AtInputAxisHandler::SetMaximum(float value) {
    m_maximum = value;
    return value;
}

float AtInputAxisHandler::SetCenter(float value) {
    m_center = value;
    return value;
}

float AtInputAxisHandler::SetDeadZone(float value) {
    m_deadZone = value;
    return value;
}

float AtInputAxisHandler::SetSaturation(float value) {
    m_saturation = value;
    return value;
}

void AtInputAxisHandler::Setup(Setting* setting) {
    Reset();
    m_mode = setting->m_mode;
    m_raw = setting->m_raw;
    m_id = setting->m_id;
    m_index = setting->m_index;
    m_minimum = setting->m_minimum;
    m_maximum = setting->m_maximum;
    m_center = setting->m_center;
    m_deadZone = setting->m_deadZone;
    m_saturation = setting->m_saturation;
    if (setting->m_deadZone > setting->m_saturation) {
        setting->m_saturation = 95.0f;
    }
    SetDeadZone(0.0f);
    SetSaturation(0.0f);
    SetMode(setting->m_mode);
    SetRaw(setting->m_raw);
    SetMinimum(setting->m_minimum);
    SetMaximum(setting->m_maximum);
    SetCenter(setting->m_center);
    SetSaturation(setting->m_saturation);
    SetDeadZone(setting->m_deadZone);
    m_flags |= 2;
    m_isSet = 1;
}

// The x87 original keeps what it does not store in a register, so those are doubles here.
float AtInputAxisHandler::Filter(float value) const {
    float center = m_center;
    const float maximum = m_maximum;
    const float minimum = m_minimum;
    if (m_mode == 1) {
        return value;
    }
    if ((double)center + kQuietBand > value && (double)center - kQuietBand < value) {
        return 0.0f;
    }
    double deadSpan = 0.0;
    float saturatedSpan = 0.0f;
    if (m_flags & 2) {
        const float span = maximum - center;
        deadSpan = (double)m_deadZone * kPercent * span;
        saturatedSpan = (float)(((double)kOne - (double)m_saturation * kPercent) * span);
    }
    const float lower = (float)((double)center - deadSpan);
    if (value < lower) {
        const double result = ((double)value - lower) / ((double)lower - minimum - saturatedSpan);
        return result < kMinusOne ? kMinusOne : (float)result;
    }
    const double upper = deadSpan + deadSpan + lower;
    if (!(value > upper)) {
        return 0.0f;
    }
    const double result = ((double)value - upper) / ((double)maximum - upper - saturatedSpan);
    return result > kOne ? kOne : (float)result;
}

void AtInputAxisHandler::Apply(float value) {
    m_previousValue = m_value;
    m_value = value;
    if (m_raw == 0) {
        m_previousFiltered = m_filtered;
        m_filtered = Filter(value);
    }
    if (m_flags & 1) {
        m_filtered = -m_filtered;
    }
}

// ---- the queue --------------------------------------------------------------------------------------------------------------

void AtInputEventQueue::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputEventQueue::AtInputEventQueue() {
    m_first = 0;
    m_last = 0;
    m_freeFirst = 0;
    m_freeLast = 0;
    m_events = 0;
    m_nodes = 0;
    m_next = 0;
    m_mask = 0;
    m_count = 0;
}

AtInputEventQueue::~AtInputEventQueue() {
    Free(m_events);
    Free(m_nodes);
    m_events = 0;
    m_nodes = 0;
    m_next = 0;
    m_mask = 0;
    m_count = 0;
}

void AtInputEventQueue::Allocate(unsigned int count, unsigned int mask) {
    Free(m_events);
    Free(m_nodes);
    m_events = 0;
    m_nodes = 0;
    m_next = 0;
    m_count = 0;
    m_mask = mask;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_events = (int*)operator new(count * 0x1C);
    AtPopHeap();
    Require(m_events);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_nodes = (Node*)operator new(count * sizeof(Node));
    AtPopHeap();
    Require(m_nodes);
    unsigned char* bytes = (unsigned char*)m_events;
    for (unsigned int i = 0; i < count * 0x1C; i++) {
        bytes[i] = 0;
    }
    for (unsigned int i = 0; i < count; i++) {
        Node* node = &m_nodes[i];
        node->m_event = (int*)((unsigned char*)m_events + i * 0x1C);
        node->m_next = m_freeFirst;
        if (m_freeFirst != 0) {
            m_freeFirst->m_prev = node;
        } else {
            m_freeLast = node;
        }
        m_freeFirst = node;
        node->m_prev = 0;
    }
}

int AtInputEventQueue::Push(const void* event) {
    Node* node = m_freeFirst;
    if (node == 0) {
        return 0;
    }
    m_freeFirst = node->m_next;
    if (m_freeFirst != 0) {
        m_freeFirst->m_prev = 0;
    } else {
        m_freeLast = 0;
    }
    const int* from = (const int*)event;
    for (int i = 0; i < 7; i++) {
        node->m_event[i] = from[i];
    }
    node->m_prev = m_last;
    if (m_last != 0) {
        m_last->m_next = node;
    } else {
        m_first = node;
    }
    m_last = node;
    node->m_next = 0;
    m_count++;
    return 1;
}

void AtInputEventQueue::Slot0(void* event) {
    if (m_mask & 1) {
        Push(event);
    }
    if (m_next != 0) {
        m_next->Slot0(event);
    }
}

void AtInputEventQueue::Slot1(void* event) {
    if (m_mask & 1) {
        Push(event);
    }
    if (m_next != 0) {
        m_next->Slot1(event);
    }
}

void AtInputEventQueue::Slot2(void* event) {
    if (m_mask & 2) {
        Push(event);
    }
    if (m_next != 0) {
        m_next->Slot2(event);
    }
}

void AtInputEventQueue::Slot3(void* a, void* b) {
    if (m_next != 0) {
        m_next->Slot3(a, b);
    }
}

void AtInputEventQueue::Unbind(AtInputController* controller, int id) {
    if (m_next != 0) {
        m_next->Unbind(controller, id);
    }
}

void AtInputEventQueue::Flush(unsigned int mask) {
    Node* node = m_first;
    while (node != 0) {
        const unsigned int wanted = node->m_event[1] == 1 ? (mask & 2) : (mask & 1);
        if (wanted == 0) {
            node = node->m_next;
            continue;
        }
        if (node->m_prev != 0) {
            node->m_prev->m_next = node->m_next;
            if (node->m_prev->m_next != 0) {
                node->m_next->m_prev = node->m_prev;
            } else {
                m_last = node->m_prev;
            }
        } else {
            m_first = node->m_next;
            if (m_first != 0) {
                m_first->m_prev = 0;
            } else {
                m_last = 0;
            }
        }
        node->m_next = m_freeFirst;
        if (m_freeFirst != 0) {
            m_freeFirst->m_prev = node;
        } else {
            m_freeLast = node;
        }
        m_freeFirst = node;
        node->m_prev = 0;
        m_count--;
        node = m_first;
    }
}

// ---- the base of a controller -------------------------------------------------------------------------------------------------

void AtInputController::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputController::AtInputController() {
    ResetFields();
}

void AtInputController::ResetFields() {
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_names = 0;
    m_inputs = 0;
    m_buttonInputs = 0;
    m_axisInputs = 0;
    m_povInputs = 0;
    m_states = 0;
    m_previousStates = 0;
    m_unknown84 = 0;
    m_buttonChains = 0;
    m_axisChains = 0;
    m_povDirections = 0;
    m_povValues = 0;
    m_previousPovValues = 0;
    m_started = 0;
    m_acquired = 0;
    m_unknownC8 = 0;
    m_unknownCC = 0;
    m_buttons = 0;
    m_axes = 0;
    m_povs = 0;
    m_unknown10 = 0;
    m_buttonInputCount = 0;
    m_axisInputCount = 0;
    m_unknownB4 = 0;
    m_unknownB8 = 0;
    m_unknownBC = 0;
    m_unknown14 = 1;
    m_unknownD0 = 1;
    ZeroInts(m_unknown88, 4);
    ZeroInts(m_unknown5C, 8);
}

int AtInputController::AllocateInputs() {
    const unsigned int povs = PovCount();
    unsigned int total = povs + AxisCount();
    total += ButtonCount();
    const unsigned int axes = AxisCount();
    const unsigned int povsAgain = PovCount();
    m_inputs = (Input*)NewFromControllerHeap(total * sizeof(Input));
    Require(m_inputs);
    m_states = (int*)NewFromControllerHeap(total * 4);
    Require(m_states);
    m_previousStates = (int*)NewFromControllerHeap(total * 4);
    Require(m_previousStates);
    m_unknown84 = (int*)NewFromControllerHeap(total * 4);
    Require(m_unknown84);
    m_buttonChains = NewChains(total);
    Require(m_buttonChains);
    m_buttonInputs = m_inputs;
    m_axisInputs = m_inputs + m_buttons;
    m_povInputs = m_axisInputs + m_axes;
    ZeroInts(m_states, total);
    ZeroInts(m_previousStates, total);
    ZeroInts(m_unknown84, total);
    for (unsigned int i = 0; i < total; i++) {
        m_inputs[i].m_unknown04 = 0;
    }
    if (axes != 0) {
        m_axisChains = NewChains(m_axisInputCount);
        Require(m_axisChains);
    }
    if (povsAgain != 0) {
        m_povValues = (float*)NewFromControllerHeap(povsAgain * 4);
        Require(m_povValues);
        m_previousPovValues = (float*)NewFromControllerHeap(povsAgain * 4);
        Require(m_previousPovValues);
        m_povDirections = (int*)NewFromControllerHeap(povsAgain * 4);
        Require(m_povDirections);
        ZeroInts((int*)m_povValues, povsAgain);
        ZeroInts((int*)m_previousPovValues, povsAgain);
        for (unsigned int i = 0; i < m_povs; i++) {
            m_povDirections[i] = 8;
        }
    }
    return 0;
}

void AtInputController::Acquire() {
    m_acquired = 1;
}

void AtInputController::Unacquire() {
    m_acquired = 0;
}

AtInputController::~AtInputController() {
    AtInputController::Release();
}

void AtInputController::TellOfLoss(int id, Chain* chain) {
    for (unsigned int i = 0; i < m_receivers.m_count; i++) {
        ((AtInputReceiver*)m_receivers.m_items[i])->Unbind(this, id);
    }
    while (chain->m_first != 0) {
        chain->m_first->m_owner->Unbind(this, id);
    }
}

int AtInputController::Release() {
    if (m_started == 0) {
        return 0;
    }
    if (m_buttonChains != 0) {
        for (unsigned int i = 0; i < m_buttonInputCount; i++) {
            TellOfLoss(m_buttonInputs[i].m_id, &m_buttonChains[i]);
        }
        operator delete(m_buttonChains);
    }
    if (m_axisChains != 0) {
        for (unsigned int i = 0; i < m_axisInputCount; i++) {
            // The same as TellOfLoss, inline, in the original.
            TellOfLoss(m_axisInputs[i].m_id, &m_axisChains[i]);
        }
        operator delete(m_axisChains);
    }
    Free(m_inputs);
    Free(m_states);
    Free(m_previousStates);
    Free(m_unknown84);
    Free(m_povValues);
    Free(m_previousPovValues);
    Free(m_povDirections);
    Unacquire();
    ResetFields();
    return 0;
}

int AtInputController::Tick(int) {
    AtFatal("AtInputController::Tick (0x000783A0) is not recovered", 0, 0, 0);
    return 0;
}

void AtInputController::InputName(int, AtText*) {
    AtFatal("AtInputController::InputName (0x000772E0) is not recovered", 0, 0, 0);
}

int AtInputController::SetUnknown1C(int value) {
    m_unknown1C = value;
    return value;
}

void AtInputController::Slot15(int, int, int) {
}

void AtInputController::Slot16(int) {
}

int AtInputController::BindButton(AtInputLink* link) {
    unsigned int index = 0;
    while (index < m_buttonInputCount && link->m_id != m_buttonInputs[index].m_id) {
        index++;
    }
    if (index >= m_buttonInputCount) {
        return 0;
    }
    Chain* chain = &m_buttonChains[index];
    link->m_next = chain->m_first;
    if (chain->m_first != 0) {
        chain->m_first->m_prev = link;
    } else {
        chain->m_last = link;
    }
    chain->m_first = link;
    link->m_prev = 0;
    return 1;
}

int AtInputController::BindAxis(AtInputLink* link) {
    unsigned int index = 0;
    while (index < m_axisInputCount && link->m_id != m_axisInputs[index].m_id) {
        index++;
    }
    if (index >= m_axisInputCount) {
        return 0;
    }
    Chain* chain = &m_axisChains[index];
    link->m_next = chain->m_first;
    if (chain->m_first != 0) {
        chain->m_first->m_prev = link;
    } else {
        chain->m_last = link;
    }
    chain->m_first = link;
    link->m_prev = 0;
    return 1;
}

int AtInputController::Start(const AtInputControllerSetting& setting) {
    SetUnknown1C(m_unknown1C);
    m_receivers.Allocate(setting.m_eventHandlers);
    Acquire();
    m_started = 1;
    return 0;
}

void AtInputController::SetValue(unsigned int index, float value) {
    if (index >= m_axisInputCount) {
        return;
    }
    if (index < m_axes) {
        AxisHandler(index)->Apply(value);
        return;
    }
    const unsigned int pov = index - m_axes;
    if (pov < m_povs) {
        m_previousPovValues[pov] = m_povValues[pov];
        m_povValues[pov] = value;
    }
}

// ---- an abstract controller -------------------------------------------------------------------------------------------------

AtAbstractController::AtAbstractController() {
    ClearHandlers();
}

AtAbstractController::~AtAbstractController() {
    AtAbstractController::Release();
}

void AtAbstractController::ClearHandlers() {
    m_unknown14 = 5;
    s_povAngles[0] = 0.0f;
    s_povAngles[1] = 0.78539819f;
    s_povAngles[2] = 1.5707964f;
    s_povAngles[3] = 2.3561945f;
    s_povAngles[4] = 3.1415927f;
    s_povAngles[5] = 3.9269910f;
    s_povAngles[6] = 4.7123890f;
    s_povAngles[7] = 5.4977875f;
    ZeroInts(m_unknown100, 4);
    ZeroInts(m_unknown110, 4);
    m_handlers = 0;
}

int AtAbstractController::Slot0() {
    return 0;
}

int AtAbstractController::CopyStates() {
    if (m_acquired == 0) {
        return 5;
    }
    const unsigned int total = PovCount() + AxisCount() + ButtonCount();
    for (unsigned int i = 0; i < total; i++) {
        m_previousStates[i] = m_states[i];
    }
    return 0;
}

int AtAbstractController::Update() {
    AtFatal("AtAbstractController::Update (0x00079B40) is not recovered", 0, 0, 0);
    return 0;
}

int AtAbstractController::AllocateInputs() {
    const int status = AtInputController::AllocateInputs();
    if (status != 0) {
        return status;
    }
    if (m_axes != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_handlers = AtNewArray<AtInputAxisHandler>(m_axes);
        AtPopHeap();
        Require(m_handlers);
    }
    for (unsigned int i = 0; i < m_buttons; i++) {
        m_buttonInputs[i].m_id = 0x30000000 + i;
    }
    for (unsigned int i = 0; i < m_axes; i++) {
        m_axisInputs[i].m_id = 0x40000000 + i;
    }
    for (unsigned int i = 0; i < m_povs; i++) {
        m_povInputs[i].m_id = 0x50000000 + i;
        SetValue(m_axes + i, kTwoPi);
    }
    return 0;
}

int AtAbstractController::Release() {
    if (m_started == 0) {
        return 0;
    }
    if (m_handlers != 0) {
        AtDeleteArray(m_handlers);
    }
    ClearHandlers();
    return AtInputController::Release();
}

void AtAbstractController::InputName(int, AtText*) {
    AtFatal("AtAbstractController::InputName (0x000795D0) is not recovered", 0, 0, 0);
}

unsigned int AtAbstractController::ButtonCount() {
    return m_buttons;
}

unsigned int AtAbstractController::AxisCount() {
    return m_axes;
}

unsigned int AtAbstractController::PovCount() {
    return m_povs;
}

AtInputAxisHandler* AtAbstractController::AxisHandler(unsigned int index) {
    return index < m_axes ? &m_handlers[index] : 0;
}

int AtAbstractController::SetUnknown1C(int) {
    return 0;
}

int AtAbstractController::StartFrom(const AtInputControllerSetting& setting) {
    m_queue.Allocate(m_buttonInputCount * 2, 0xFF);
    int status = Start(setting);
    if (status != 0) {
        return status;
    }
    status = AllocateInputs();
    if (status != 0) {
        return status;
    }
    for (unsigned int i = 0; i < m_axes; i++) {
        AtInputAxisHandler::Setting handler;
        handler.m_mode = m_unknown1C;
        handler.m_raw = 1;
        handler.m_id = m_axisInputs[i].m_id;
        handler.m_index = m_buttons + i;
        handler.m_minimum = -10000.0f;
        handler.m_maximum = 10000.0f;
        handler.m_center = 0.0f;
        handler.m_deadZone = setting.m_deadZone;
        handler.m_saturation = setting.m_saturation;
        m_handlers[i].Setup(&handler);
    }
    return 0;
}

int AtAbstractController::Init(const AtInputControllerSetting& setting) {
    const int status = Release();
    if (status != 0) {
        return status;
    }
    m_buttons = setting.m_buttons;
    m_axes = setting.m_axes;
    m_povs = setting.m_povs;
    m_buttonInputCount = m_buttons + m_axes + m_povs;
    m_axisInputCount = m_axes + m_povs;
    return StartFrom(setting);
}

void AtAbstractController::Read(void*, AtIoTokenizer* tokenizer) {
    AtInputControllerSetting setting;
    setting.m_unknown00 = 0;
    setting.m_unknown04 = 0;
    setting.m_eventHandlers = 0;
    setting.m_deadZone = 0.0f;
    setting.m_saturation = 100.0f;
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kButtons:
            m_buttons = (unsigned int)tokenizer->ExpectInt();
            m_buttonInputCount = m_povs + m_axes + m_buttons;
            break;
        case kAxes:
            m_axes = (unsigned int)tokenizer->ExpectInt();
            m_buttonInputCount = m_buttons + m_axes + m_povs;
            m_axisInputCount = m_axes + m_povs;
            break;
        case kPovs:
            m_povs = (unsigned int)tokenizer->ExpectInt();
            m_buttonInputCount = m_buttons + m_axes + m_povs;
            m_axisInputCount = m_axes + m_povs;
            break;
        case kEventHandlers:
            setting.m_eventHandlers = (unsigned int)tokenizer->ExpectInt();
            break;
        case kDeadZone:
            setting.m_deadZone = tokenizer->ExpectReal();
            break;
        case kSaturation:
            setting.m_saturation = tokenizer->ExpectReal();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    StartFrom(setting);
}

// ---- the database -------------------------------------------------------------------------------------------------------

const AtIoKeyword AtInputControllerDatabase::s_keywords[AtInputControllerDatabase::kKeywordCount] = {
    {kNameSpace, "NameSpace"},
    {kAbstractController, "AbstractController"},
    {kButtons, "Buttons"},
    {kAxes, "Axes"},
    {kPovs, "POVs"},
    {kEventHandlers, "EventHandlers"},
    {kDeadZone, "DeadZone"},
    {kSaturation, "Saturation"},
};

void AtInputOwner::AddControllers(GmNameMap* map, const char* ns) {
    if (ns == 0) {
        m_controllers.Add(map);
    } else {
        m_controllerSpaces.Add(ns, map);
    }
}

void AtInputControllerDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputControllerDatabase::AtInputControllerDatabase() {
    m_unknown18 = 0;
    m_items = 0;
    m_unknown1C = 0;
}

AtInputControllerDatabase::~AtInputControllerDatabase() {
    if (m_items != 0) {
        AtDeleteArray(m_items);
        m_unknown18 = 0;
        m_items = 0;
        m_unknown1C = 0;
        GmNameMap::Clear();
    }
}

void AtInputControllerDatabase::Clear() {
    if (m_items != 0) {
        AtDeleteArray(m_items);
        m_unknown18 = 0;
        m_items = 0;
        m_unknown1C = 0;
        GmNameMap::Clear();
    }
}

void AtInputControllerDatabase::Read(AtInputOwner* owner, AtIoTokenizer* tokenizer) {
    const char* ns = 0;
    char space[8];
    Clear();
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    if (token != kAbstractController) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const unsigned int count = (unsigned int)tokenizer->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_items = AtNewArray<AtAbstractController>(count);
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(count);
    owner->AddControllers(this, ns);
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->ExpectToken(kAbstractController);
        char name[8];
        tokenizer->ExpectName(name);
        m_items[i].Read(m_unknown18, tokenizer);
        Insert(name, &m_items[i]);
    }
    tokenizer->ExpectRightCurly();
}

void AtInputControllerDatabase::Load(AtInputOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ANA", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ANA");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
