// Stands for GmSoundNoteItem, recovered on branch worktree-agent-ab39b088b154f53ed (include/gm/GmSoundNotes.h), which this branch does not merge. Not recovered code here: each stops with a
// diagnostic. When the branches meet, replace each body with the real call: new GmSoundNoteItem[count] (as the parser's other arrays), &items[index], item->Configure((const
// GmSoundNoteItem::Request*)request), GmAudioData::s_instance->RegisterNotes(item->m_bank), and delete[] as the parser's other arrays.
#include "gm/GmLevelManifest.h"

void AtFatal(const char* message, int a, int b, int c);

GmSoundNoteItem* GmSoundNoteItemsNew(unsigned int) {
    AtFatal("Level manifest: sound notes are on another branch", 0, 0, 0);
    return 0;
}

GmManifestItem* GmSoundNoteItemAt(GmSoundNoteItem*, unsigned int) {
    AtFatal("Level manifest: sound notes are on another branch", 0, 0, 0);
    return 0;
}

void GmSoundNoteItemConfigure(GmManifestItem*, const GmManifestItemRequest*) {
    AtFatal("Level manifest: sound notes are on another branch", 0, 0, 0);
}

void GmSoundNoteItemRegister(GmManifestItem*) {
    AtFatal("Level manifest: sound notes are on another branch", 0, 0, 0);
}

void GmSoundNoteItemsDelete(GmSoundNoteItem*) {
    AtFatal("Level manifest: sound notes are on another branch", 0, 0, 0);
}
