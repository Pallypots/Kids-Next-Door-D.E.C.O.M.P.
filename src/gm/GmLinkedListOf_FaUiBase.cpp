// Explicit instantiation of the GmLinkedListOf<FaUiBase*> members whose bodies are recovered.
// The original translation unit that instantiated them is unknown (template code is emitted wherever used).
#include "gm/GmLinkedList.h"

template void GmLinkedListOf<FaUiBase*>::Remove(FaUiBase*);
template int GmLinkedListOf<FaUiBase*>::Contains(FaUiBase*);
template int GmLinkedListOf<FaUiBase*>::IsEmpty();
