# The fatal error path's mutants in src/gm: the rumble playback, the controller module and the error screen. The rest is tests/oracle/suites/fatal.py; one oracle runs both.
# Each text occurs exactly once in its file.
#
# Dropped as equivalent, each with its reason:
#  * The order of the four stores in GmRumblePlayback::Clear: nothing reads between them.
#  * Intensity's test `found + 1 >= FrameCount()` against `>`: both branches return the same frame's value; the other only also takes the address of the frame past the
#    end, which it never reads, as the original takes the next one's and never reads it.
#  * The ticker seam bodies of GmCoTicker: the oracle's tickers override the one slot called, and none is code of the game.
R = "GmRumblePlayback.cpp"
M = "GmControllerModule.cpp"
S = "GmFatalScreen.cpp"

PLAYBACK = [
    ("duration: last", R, "return m_frames.m_items[m_frames.m_count - 1].m_time;", "return m_frames.m_items[0].m_time;"),
    ("frame count", R, "    return m_frames.m_count;\n", "    return m_frames.m_count + 1;\n"),
    ("stop: loop", R, "void GmRumblePlayback::Stop() {\n    m_loop = 0;", "void GmRumblePlayback::Stop() {"),
    ("stop: time", R, "    m_time = m_effect->Duration();\n}", "    m_time = 0.0f;\n}"),
    ("advance: sum", R, "m_time = (float)((double)seconds + m_time);", "m_time = (float)((double)seconds);"),
    ("advance: loop test", R, "if (m_loop != 0 && m_effect->Duration() < m_time) {", "if (m_loop != 0 && m_effect->Duration() <= m_time) {"),
    ("advance: loop ignored", R, "if (m_loop != 0 && m_effect->Duration() < m_time) {", "if (m_effect->Duration() < m_time) {"),
    ("advance: wrap", R, "m_time = (float)((double)m_time - m_effect->Duration());", "m_time = 0.0f;"),
    ("intensity: strict", R, "if (m_effect->Frame(i)->m_time < m_time) {", "if (m_effect->Frame(i)->m_time <= m_time) {"),
    ("intensity: first", R, "            found = i;\n", "            found = i > 0 ? i - 1 : 0;\n"),
    ("intensity: last right", R, "        GmRumbleFrame* frame = m_effect->Frame(found);\n        return motor == 1 ? frame->m_right : frame->m_left;\n    }",
     "        GmRumbleFrame* frame = m_effect->Frame(found);\n        return motor == 2 ? frame->m_right : frame->m_left;\n    }"),
    ("intensity: blend", R, "    m_effect->Frame(found + 1);  // ORIGINAL: the next frame is fetched and not used: no blending between frames\n    return motor == 1 ? frame->m_right : frame->m_left;",
     "    GmRumbleFrame* next = m_effect->Frame(found + 1);\n    return motor == 1 ? (frame->m_right + next->m_right) * 0.5f : frame->m_left;"),
    ("finished: loop", R, "if (m_loop == 0 && m_effect->Duration() <= m_time) {", "if (m_effect->Duration() <= m_time) {"),
    ("finished: equal", R, "if (m_loop == 0 && m_effect->Duration() <= m_time) {", "if (m_loop == 0 && m_effect->Duration() < m_time) {"),
    ("finished: result", R, "        return 1;\n", "        return 2;\n"),
    ("clear: effect", R, "    m_effect = 0;\n", ""),
    ("clear: time", R, "    m_time = 0.0f;\n    m_loop = 0;\n}", "    m_loop = 0;\n}"),
    ("clear: word 0", R, "    m_unknown00 = 0;\n", ""),
]

MODULE = [
    ("value at", M, "return m_entries[index].m_value;", "return m_entries[index + 1].m_value;"),
    ("instance", M, "    return BaseInstance();\n", "    return 0;\n"),
    ("maps: three", M, "for (int map = 0; map < 4; map++) {", "for (int map = 1; map < 4; map++) {"),
    ("maps: which", M, "GmNameMap& tickers = m_tickers[map];", "GmNameMap& tickers = m_maps68[map];"),
    ("ticks: one fewer", M, "for (unsigned int i = 0; i < tickers.m_size; i++) {", "for (unsigned int i = 1; i < tickers.m_size; i++) {"),
    ("ticks: time", M, "ticker->Tick(time);", "ticker->Tick(time / 2);"),
    ("manager: not forced", M, "GmRumbleSystem::s_instance->m_manager->Update(time, 1);", "GmRumbleSystem::s_instance->m_manager->Update(time, 0);"),
    ("manager: time", M, "->Update(time, 1);", "->Update(time + 1, 1);"),
    ("manager: none", M, "    GmRumbleSystem::s_instance->m_manager->Update(time, 1);\n", ""),
]

SCREEN = [
    ("screen: not loaded", S, "if (m_dirtyDiskLoader == 0) {", "if (m_dirtyDiskLoader == 0 && 0) {"),
    ("screen: images index", S, "m_dirtyDiskLoader->ImageAt(0);", "m_dirtyDiskLoader->ImageAt(1);"),
    ("screen: no camera attach", S, "    camera->Attach(owner);\n", ""),
    ("screen: german", S, "first = 2;", "first = 3;"),
    ("screen: french", S, "first = 4;", "first = 2;"),
    ("screen: italian", S, "first = AtCompareName8(language, \"ITALIAN\") == 0 ? 6 : 0;", "first = AtCompareName8(language, \"ITALIAN\") == 0 ? 0 : 6;"),
    ("screen: french name", S, "\"FRENCH\"", "\"FRANCE\""),
    ("screen: second image", S, "images->ImageAt(first + 1);", "images->ImageAt(first);"),
    ("screen: left place", S, "    leftAt.x = 0.0f;\n", "    leftAt.x = 1.0f;\n"),
    ("screen: right place", S, "rightAt.x = 512.0f;", "rightAt.x = 640.0f;"),
    ("screen: right height", S, "    rightAt.y = 0.0f;\n", "    rightAt.y = 1.0f;\n"),
    ("screen: begin", S, "renderer->Begin(camera, 7, 0);", "renderer->Begin(camera, 6, 0);"),
    ("screen: begin camera", S, "renderer->Begin(camera, 7, 0);", "renderer->Begin(0, 7, 0);"),
    ("screen: left drawn", S, "        left->Draw(renderer, &leftAt, 0, 0, 0);\n", ""),
    ("screen: right swapped", S, "right->Draw(renderer, &rightAt, 0, 0, 0);", "right->Draw(renderer, &leftAt, 0, 0, 0);"),
    ("screen: no end", S, "        renderer->End();\n", ""),
    ("screen: present", S, "owner->Present(1);", "owner->Present(0);"),
    ("screen: language source", S, "AtCopyName8(language, s_instance->m_settings.m_language);", "AtCopyName8(language, \"ENGLISH\");"),
]

SUITE = {"target": "oracle_fatal", "subsystem": "gm/fatal", "src": "gm", "var": "KND_GM_SRC", "cases": "none", "mutants": PLAYBACK + MODULE + SCREEN}
