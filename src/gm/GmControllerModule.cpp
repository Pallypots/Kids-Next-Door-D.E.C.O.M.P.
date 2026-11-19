// The controller module's update and its instance (App+0x692C, RTTI FaCoControllerModule: include/gm/GmApp.h), and the name map's value by index it reads the tickers with.
// Original addresses 0x0006DEA0, 0x0006DA50, 0x0008F4F0 and 0x00024B70. Verification status per function: config/functions.d/fatal.toml.
//
// In a file of its own: tests/oracle/oracle_app.cpp supplies its own AppUnknown692C::Update, and must not pull this one in for something else.
#include "at/AtControllerManager.h"
#include "gm/GmApp.h"

AppUnknown692C* AppUnknown692C::s_instance;

namespace {
void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

void* GmNameMap::ValueAt(unsigned int index) {
    return m_entries[index].m_value;
}

AppUnknown692C* AppUnknown692C::BaseInstance() {
    return s_instance;
}

AppUnknown692C* AppUnknown692C::Instance() {
    return BaseInstance();
}

void AppUnknown692C::Update(int time) {
    for (int map = 0; map < 4; map++) {
        GmNameMap& tickers = m_tickers[map];
        for (unsigned int i = 0; i < tickers.m_size; i++) {
            GmCoTicker* ticker = (GmCoTicker*)tickers.ValueAt(i);
            if (ticker != 0) {
                ticker->Tick(time);
            }
        }
    }
    GmRumbleSystem::s_instance->m_manager->Update(time, 1);
}

void GmCoTicker::Slot0() { NotRecovered("Gm controller module: a ticker's slot 0 is not recovered"); }
void GmCoTicker::Slot1() { NotRecovered("Gm controller module: a ticker's slot 1 is not recovered"); }
void GmCoTicker::Slot2() { NotRecovered("Gm controller module: a ticker's slot 2 is not recovered"); }
void GmCoTicker::Slot3() { NotRecovered("Gm controller module: a ticker's slot 3 is not recovered"); }
void GmCoTicker::Slot4() { NotRecovered("Gm controller module: a ticker's slot 4 is not recovered"); }
void GmCoTicker::Slot5() { NotRecovered("Gm controller module: a ticker's slot 5 is not recovered"); }
void GmCoTicker::Slot6() { NotRecovered("Gm controller module: a ticker's slot 6 is not recovered"); }
void GmCoTicker::Slot7() { NotRecovered("Gm controller module: a ticker's slot 7 is not recovered"); }
void GmCoTicker::Tick(int) { NotRecovered("Gm controller module: a ticker's tick (slot 8) is not recovered"); }
