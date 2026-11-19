// App's fatal error screen, 0x0008E2B0: see include/gm/GmApp.h. Verification status: config/functions.d/fatal.toml.
//
// The screen is the dirty-disc screen start-up loads (App::LoadDirtyDiskScreen): two images side by side, a pair for each language, drawn with the graphics layer's 2D renderer
// until the machine is switched off. The loop never ends; what the graphics layer does each frame is behind seams (include/at/AtGr2D.h, AtMaterialOwner::Present).
#include "at/AtDatabaseLoader.h"
#include "at/AtGr2D.h"
#include "at/AtImageDatabase.h"
#include "gm/GmApp.h"
#include "gm/GmLevelManager.h"

void App::DrawFatalScreen() {
    if (m_dirtyDiskLoader == 0) {
        return;
    }
    AppGraphics* graphics = s_instance->m_graphics;
    AtMaterialOwner* owner = s_instance->m_materialOwner;
    AtImageDatabase* images = m_dirtyDiskLoader->ImageAt(0);
    AtGr2DRenderer* renderer = s_instance->m_renderer2D;
    AtCamera* camera = graphics->Camera2D();
    camera->Attach(owner);
    char language[8];
    AtCopyName8(language, s_instance->m_settings.m_language);
    unsigned int first;  // English, and any language not named here
    if (AtCompareName8(language, "GERMAN") == 0) {
        first = 2;
    } else if (AtCompareName8(language, "FRENCH") == 0) {
        first = 4;
    } else {
        first = AtCompareName8(language, "ITALIAN") == 0 ? 6 : 0;
    }
    AtImage* left = images->ImageAt(first);
    AtImage* right = images->ImageAt(first + 1);
    AtVector2 leftAt;
    leftAt.x = 0.0f;
    leftAt.y = 0.0f;
    AtVector2 rightAt;
    rightAt.x = 512.0f;
    rightAt.y = 0.0f;
    for (;;) {
        renderer->Begin(camera, 7, 0);
        left->Draw(renderer, &leftAt, 0, 0, 0);
        right->Draw(renderer, &rightAt, 0, 0, 0);
        renderer->End();
        owner->Present(1);
    }
}
