// What the math, data management, script and kinematics layers reach and is not recovered (at/atlayers). Each stops with a diagnostic; none is
// recorded or counted. In a file of its own, so that a harness can supply them instead. See include/at/AtApMidLayers.h.
#include "at/AtApMidLayers.h"

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

void AtMathSystem::DeletingDestructor(unsigned int) { StopHere("Math layer: the deleting destructor (0x000B3910) is not recovered"); }
void AtDataManagementSystem::DeletingDestructor(unsigned int) { StopHere("Data management layer: the deleting destructor (0x000B3850) is not recovered"); }
void AtDataRegistryOwner::DeletingDestructor(unsigned int) { StopHere("AtDataRegistryOwner: the deleting destructor (0x0005F260) is not recovered"); }
void AtScriptSystem::DeletingDestructor(unsigned int) { StopHere("Script layer: the deleting destructor (0x0009EF50) is not recovered"); }
void AtKinematicsSystemBase::DeletingDestructor(unsigned int) { StopHere("Kinematics layer: the base's deleting destructor (0x000C87A0) is not recovered"); }
void AtKinematicsSystem::DeletingDestructor(unsigned int) { StopHere("Kinematics layer: the deleting destructor (0x000B3670) is not recovered"); }

void AtKinematicsFactory::Slot0() { StopHere("Kinematics factory: slot 0 (0x000C87E0) is not recovered"); }
void AtKinematicsFactory::Slot1() { StopHere("Kinematics factory: slot 1 (0x000C8940) is not recovered"); }
void AtKinematicsFactory::Slot2() { StopHere("Kinematics factory: slot 2 (0x000C8880) is not recovered"); }
void AtKinematicsFactory::Slot3() { StopHere("Kinematics factory: slot 3 (0x000EB9D0) is not recovered here"); }
void AtKinematicsFactory::Slot4() { StopHere("Kinematics factory: slot 4 (0x000C8F50) is not recovered"); }
void AtKinematicsFactory::Slot5() { StopHere("Kinematics factory: slot 5 (0x000C91E0) is not recovered"); }
void AtKinematicsFactory::Slot6() { StopHere("Kinematics factory: slot 6 (0x000C8FF0) is not recovered"); }

// The built-ins of the Small language's core and float modules, which start-up only registers.
#define NATIVE(Name, Va) \
    int Name(void*, AtAmx*, int*) { StopHere("Small-C scripts: the built-in " #Name " (" #Va ") is not recovered"); return 0; }
NATIVE(AtSmallCNumArgs, 0x0007CA10)
NATIVE(AtSmallCGetArg, 0x0007CA30)
NATIVE(AtSmallCSetArg, 0x0007CA60)
NATIVE(AtSmallCHeapSpace, 0x0007CAB0)
NATIVE(AtSmallCFuncIdx, 0x0007CAC0)
NATIVE(AtSmallCStrLen, 0x0007CC30)
NATIVE(AtSmallCStrPack, 0x0007CC70)
NATIVE(AtSmallCStrUnpack, 0x0007CD10)
NATIVE(AtSmallCToLower, 0x0007CDB0)
NATIVE(AtSmallCToUpper, 0x0007CDD0)
NATIVE(AtSmallCFloat, 0x0007C800)
NATIVE(AtSmallCFloatStr, 0x0007C810)
NATIVE(AtSmallCFloatMul, 0x0007C870)
NATIVE(AtSmallCFloatDiv, 0x0007C8A0)
NATIVE(AtSmallCFloatAdd, 0x0007C8D0)
NATIVE(AtSmallCFloatSub, 0x0007C900)
NATIVE(AtSmallCFloatFract, 0x0007C930)
NATIVE(AtSmallCFloatRound, 0x0007C960)
NATIVE(AtSmallCFloatCmp, 0x0007C9D0)
#undef NATIVE
