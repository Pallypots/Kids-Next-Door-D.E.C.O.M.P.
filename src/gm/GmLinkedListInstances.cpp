// Forces emission of compiler-generated code for the one recovered instantiation so it can be compared. The original
// translation units holding these are unknown: the template and the iterator destructor are emitted wherever used.
#include "gm/GmLinkedList.h"

class FaUiBase : public GmPlug {};  // Stand-in: only the base class is known here. Real class: FaUiBase (RTTI).

GmLinkedList* EmitLinkedListInstances() {
    GmLinkedListOf<FaUiBase*>* list = new GmLinkedListOf<FaUiBase*>;
    GmLinkedListIterator iterator(list);
    (void)iterator;
    delete list;
    return 0;
}
