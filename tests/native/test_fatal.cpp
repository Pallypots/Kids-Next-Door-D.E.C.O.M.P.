// Host test of the game's fatal error path, x64 or any host: the report, the rumble it stops, the error screen for a bounded number of frames, and the way out. See
// include/at/AtFatal.h.
//
// The port's functions platform/fatal.cpp holds are defined here instead, and AtFatal is not: the game's own is linked, and its exit is caught as an exception.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtControllerManager.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtFatal.h"
#include "at/AtGr2D.h"
#include "at/AtImageDatabase.h"
#include "at/AtMatrix.h"
#include "gm/GmApp.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::string g_log;
static void note(const char* format, ...) {
    char text[200];
    va_list a;
    va_start(a, format);
    std::vsnprintf(text, sizeof text, format, a);
    va_end(a);
    g_log += text;
    g_log += ';';
}

struct Exit {
    int code;
};
struct FrameLimit {};

// ---- the port's functions ----------------------------------------------------------------------------------------------
static std::string g_printed;
static unsigned int g_launch[0x300];
static int g_launches;
void AtPlatformLaunchImage(const char* path, AtLaunchData* data) {
    note("launch %d", path == nullptr);
    g_launches++;
    std::memcpy(g_launch, data->m_words, sizeof g_launch);
}
int AtLibraryPrint(const char* format, ...) {
    char text[1024];
    va_list a;
    va_start(a, format);
    const int n = std::vsnprintf(text, sizeof text, format, a);
    va_end(a);
    g_printed = text;
    note("print");
    return n;
}
void AtLibraryExit(int code) {
    note("exit %d", code);
    throw Exit{code};
}
void* AtPlatformAllocateRegion(unsigned int size) { return std::malloc(size); }
void AtPlatformFreeRegion(void* region) { std::free(region); }
void AtPlatformInitDevices() {}
void AtLibraryFree(void* data) { std::free(data); }
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }
unsigned int AtPlatformMilliseconds() { return 0; }

static void reset() {
    g_log.clear();
    g_printed.clear();
    g_launches = 0;
    std::memset(g_launch, 0xEE, sizeof g_launch);
    g_atFatalScreenShown = 0;
    g_atFatalExiting = 0;
    AtControllerManager::s_ready = 0;
    AtControllerManager::s_instance = nullptr;
    AppUnknown692C::s_instance = nullptr;
    g_app.m_dirtyDiskLoader = nullptr;
}

static int run_fatal(const char* message, int line) {
    try {
        AtFatal(message, 0, line, 0);
    } catch (const Exit& e) {
        return e.code;
    }
    return -1;
}

// ---- the way out, and the report ---------------------------------------------------------------------------------------
static void test_way_out() {
    reset();
    CHECK(run_fatal("boom", 7) == 1);
    CHECK(g_log == "launch 1;print;exit 1;");
    CHECK(g_printed == "boom, file \"(null)\" line 7\n");
    CHECK(g_launch[0] == 1 && g_launch[1] == 0 && g_launch[2] == 5 && g_launch[3] == 0 && g_launch[0x2FF] == 0);
    CHECK(g_atFatalScreenShown == 1 && g_atFatalExiting == 1);

    // A long message runs past the text the original makes and never uses; the launch data has gone by then.
    std::string longer(0x1F0, 'x');
    reset();
    CHECK(run_fatal(longer.c_str(), 1) == 1);
    CHECK(g_printed == longer + ", file \"(null)\" line 1\n");

    reset();
    try { AtReport(1, "%s.%s", "DIR\\NAME", "AGT"); } catch (const Exit&) {}
    CHECK(g_printed == "file or directory not found: DIR\\NAME.AGT, file \"(null)\" line 0\n");
    reset();
    try { AtReport(99, nullptr); } catch (const Exit&) {}
    CHECK(g_printed == "<unspecified error>: <No Message>, file \"(null)\" line 0\n");
    reset();
    try { AtReport(0, "%d-%d", -3, 12); } catch (const Exit&) {}
    CHECK(g_printed == "no error: -3-12, file \"(null)\" line 0\n");
}

// ---- the rumble --------------------------------------------------------------------------------------------------------
class Phys : public AtInputPhysicalController {
public:
    int Slot0() override { return 0; }
    int CopyStates() override { return 0; }
    int Update() override { return 0; }
    unsigned int ButtonCount() override { return 0; }
    unsigned int AxisCount() override { return 0; }
    unsigned int PovCount() override { return 0; }
    AtInputAxisHandler* AxisHandler(unsigned int) override { return nullptr; }
    void PhysicalSlot0() override {}
    void Slot15(int motor, int bits, int ms) override {
        float f;
        std::memcpy(&f, &bits, 4);
        note("motor %d %g %d", motor, f, ms);
    }
    void Slot16(int motor) override { note("stop %d", motor); }
};

class Dev : public AtInputDevice {
public:
    Phys phys;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void Slot13() override {}
    int Update(int ms) override { note("device %d", ms); return 0; }
    void Slot15() override {}
    void Slot16() override {}
    void Slot17() override {}
    unsigned int PhysicalCount() override { return 1; }
    unsigned int ConnectedCount() override { return 1; }
    AtInputPhysicalController* First() override { return &phys; }
    AtInputPhysicalController* Second() override { return &phys; }
    AtInputPhysicalController* Numbered(unsigned int) override { return &phys; }
    AtInputPhysicalController* Physical(unsigned int i) override { note("physical %u", i); return &phys; }
};

class Controller : public AtAbstractController {
public:
    int Tick(int ms) override { note("controller tick %d", ms); return 0; }
};

class Ticker : public GmCoTicker {
public:
    void Tick(int ms) override { note("ticker %d", ms); }
};

static void resume(void* context) { note("resume %d", *(int*)context); }

static void test_rumble() {
    reset();
    GmRumbleFrame frames[2];
    frames[0].m_time = 0.0f; frames[0].m_left = 0.5f; frames[0].m_right = 0.25f;
    frames[1].m_time = 1.0f; frames[1].m_left = 1.0f; frames[1].m_right = 0.75f;
    GmRumbleEffect effect;
    effect.m_frames.m_items = frames;
    effect.m_frames.m_count = 2;
    CHECK(effect.Duration() == 1.0f && effect.FrameCount() == 2);

    static GmRumblePlayback pool[10];
    std::memset(pool, 0, sizeof pool);
    pool[0].m_effect = &effect; pool[0].m_time = 0.3f; pool[0].m_loop = 1;
    pool[1].m_effect = &effect; pool[1].m_time = 0.1f; pool[1].m_loop = 0;

    // The pieces first, on copies.
    GmRumblePlayback p = pool[0];
    CHECK(p.Intensity(0) == 0.5f && p.Intensity(1) == 0.25f && p.Intensity(2) == 0.5f);
    p.Advance(0.9f);  // past the end, looping: back by one length
    CHECK(p.m_time == (float)((double)(float)((double)0.9f + 0.3f) - 1.0) && p.Finished() == 0);
    p.m_time = 0.0f;
    CHECK(p.Intensity(0) == 0.5f);  // nothing strictly before 0: the first frame
    p.m_loop = 0;
    p.m_time = 1.0f;
    CHECK(p.Finished() == 1);
    p.m_time = 0.999f;
    CHECK(p.Finished() == 0);

    static Dev device;
    device.phys.m_unknown10 = 2;
    static Controller physicalController, plainController;
    AtInputControllerSlot slots[2];
    std::memset(slots, 0, sizeof slots);
    slots[0].m_controller = &physicalController;
    slots[0].m_physical = 1;
    slots[1].m_controller = &plainController;
    AtInputControllerSlot* slotTable[3] = {&slots[0], nullptr, &slots[1]};

    static AtControllerManager manager;
    AtRumblePlayer player;
    std::memset(&player, 0, sizeof player);
    player.m_enabled = 1;
    player.m_port = 3;
    player.m_manager = &manager;
    player.m_motorsOn = 1;
    for (int i = 0; i < 10; i++) player.m_playing[i] = &pool[i];
    player.m_count = 2;
    int context = 42;
    manager.m_device = &device;
    manager.m_slots = slotTable;
    manager.m_slotCount = 3;
    manager.m_players = &player;
    manager.m_playerCount = 1;
    manager.m_suspended = 1;
    manager.m_resume = resume;
    manager.m_resumeContext = &context;
    CHECK(manager.Update(5, 0) == 5 && g_log.empty());  // suspended and not forced: nothing

    static AppUnknown692C module;
    Ticker ticker;
    static GmNameMap::Entry entries[3];  // filled by hand: a map's Init takes from the game's heap, which this test has none of
    std::memset(entries, 0, sizeof entries);
    std::memcpy(entries[1].m_name, "TICK", 4);
    entries[1].m_value = &ticker;
    module.m_tickers[2].m_entries = entries;
    module.m_tickers[2].m_size = 3;
    GmRumbleSystem system;
    std::memset(&system, 0, sizeof system);
    system.m_manager = &manager;
    GmRumbleSystem::s_instance = &system;

    AtControllerManager::s_ready = 1;
    AtControllerManager::s_instance = &manager;
    AppUnknown692C::s_instance = &module;
    CHECK(AppUnknown692C::Instance() == &module);
    CHECK(run_fatal("rumble", 0) == 1);
    CHECK(g_log == "ticker 10000;resume 42;device 10000;controller tick 10000;physical 3;physical 3;motor 0 2 5000;physical 3;motor 1 1.5 5000;launch 1;print;exit 1;");
    CHECK(player.m_count == 0);
    for (int i = 0; i < 8; i++) CHECK(player.m_playing[i] == &pool[i + 2]);
    CHECK(player.m_playing[8] == &pool[1] && player.m_playing[9] == &pool[0]);
    CHECK(pool[0].m_effect == nullptr && pool[0].m_time == 0.0f && pool[1].m_effect == nullptr);
    module.m_tickers[2].m_entries = nullptr;  // not the heap's, so not for the map's destructor
    module.m_tickers[2].m_size = 0;

    // One motor carries both sides; paused players stop the motors and keep their time.
    reset();
    pool[0].m_effect = &effect; pool[0].m_time = 0.5f; pool[0].m_loop = 1;
    player.m_playing[0] = &pool[0];
    player.m_count = 1;
    device.phys.m_unknown10 = 1;
    player.Update(0.25f);
    CHECK(g_log == "physical 3;physical 3;motor 0 0.75 5000;");
    CHECK(pool[0].m_time == 0.75f && player.m_count == 1);
    reset();
    player.m_paused = 1;
    player.Update(0.25f);
    CHECK(g_log == "physical 3;physical 3;stop 0;");
    CHECK(pool[0].m_time == 0.75f);
    player.m_paused = 0;
    player.m_enabled = 0;
    reset();
    player.Update(1.0f);
    CHECK(g_log.empty());
    effect.m_frames.m_items = nullptr;  // not the heap's, so not for the effect's destructor
    effect.m_frames.m_count = 0;
}

// ---- the error screen --------------------------------------------------------------------------------------------------
static AtMatrix g_matrices[4];

class Matrices : public AtMatrixDevice {
public:
    int next = 0;
    AtMatrix* NewMatrix() override { return nullptr; }
    AtMatrixPool* NewPool() override { return nullptr; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override {}
    AtMatrix* DoNewMatrix() override { note("matrix"); return &g_matrices[next++]; }
};

class Owner : public AtMaterialOwner {
public:
    Matrices matrices;
    int frames = 0;
    AtMatrixDevice* MatrixDevice() override { note("device"); return &matrices; }
    void Present(int a) override {
        note("present %d", a);
        if (++frames == 3) throw FrameLimit();
    }
};

class Renderer : public AtGr2DRenderer {
public:
    void Begin(AtCamera* camera, int a, int b) override { note("begin %d %d %d", camera != nullptr, a, b); }
    void End() override { note("end"); }
};

class Material : public AtMaterial {
public:
    void OnLoaded(AtMaterialOwner*) override {}
};
static Material g_material;

class Image : public AtImage {
public:
    int id = 0;
    AtMaterial* Material() override { note("material %d", id); return &g_material; }
    void Slot1() override {}
    void Slot2() override {}
    void DrawStrip(AtGr2DRenderer*, unsigned int strip, AtMaterial* material, const AtRect* r, const AtRect* uv, const AtColorRectangle* c) override {
        note("strip %d.%u %d [%g %g %g %g] [%g %g %g %g] %d", id, strip, material == &g_material, r->left, r->top, r->right, r->bottom, uv->left, uv->top, uv->right, uv->bottom,
             c->m_colors[3].b);
    }
    void OnLoaded(AtMaterialOwner*) override {}
    void Slot14() override {}
};

class Images : public AtImageDatabase {
public:
    Image* items[8] = {};
    void MakeItems() override {}
    void OnReady(AtMaterialOwner*) override {}
    AtImage* ImageAt(unsigned int i) override { note("image %u", i); return items[i]; }
};

class Loader : public AtDatabaseLoader {
public:
    Images* images = nullptr;
    void MakeTextures() override {}
    void MakeMaterials() override {}
    void MakeShaders() override {}
    void MakeMeshes() override {}
    void MakeLights() override {}
    void Slot5() override {}
    void MakeFonts() override {}
    void MakeImages() override {}
    void MakeVertexProcesses() override {}
    void MakePixelProcesses() override {}
    void MakeLoaders() override {}
    AtTextureDatabase* TextureAt(unsigned int) override { return nullptr; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return nullptr; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return nullptr; }
    AtMeshDatabase* MeshAt(unsigned int) override { return nullptr; }
    AtLightDatabase* LightAt(unsigned int) override { return nullptr; }
    AtFontDatabase* FontAt(unsigned int) override { return nullptr; }
    AtImageDatabase* ImageAt(unsigned int i) override { note("images %u", i); return images; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return nullptr; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return nullptr; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return nullptr; }
};

class Graphics : public AppGraphics {
public:
    AtCamera* camera = nullptr;
    AtCamera* Camera2D() override { note("camera"); return camera; }
};

static void test_screen() {
    static Owner owner;
    static Renderer renderer;
    static Images images;
    static Loader loader;
    static Graphics graphics;
    static AtCamera camera;
    static Image left, right;
    static AtImageStrip strips[2];
    strips[0].x0 = 0; strips[0].y0 = 0; strips[0].x1 = 256; strips[0].y1 = 480;
    strips[1].x0 = 256; strips[1].y0 = 0; strips[1].x1 = 512; strips[1].y1 = 480;
    for (Image* image : {&left, &right}) {
        image->m_flags = AtImage::kOwnsData;
        image->m_unknown14 = 512;
        image->m_unknown16 = 480;
        image->m_unknown1E = 2;
        image->m_data = strips;
    }
    left.id = 4;
    right.id = 5;
    g_material.m_constant.r = 10; g_material.m_constant.g = 20; g_material.m_constant.b = 30; g_material.m_constant.a = 40;
    images.items[4] = &left;
    images.items[5] = &right;
    loader.images = &images;
    graphics.camera = &camera;
    renderer.m_flags = 0;
    renderer.m_viewport[0] = 0; renderer.m_viewport[1] = 0; renderer.m_viewport[2] = 640; renderer.m_viewport[3] = 480;
    renderer.m_material = nullptr;

    reset();
    App::s_instance = &g_app;
    g_app.m_device->m_factory = &graphics;
    g_app.m_device->m_materialOwner = &owner;
    g_app.m_device->m_unknown20 = &renderer;
    std::memcpy(g_app.m_settings.m_language, "FRENCH\0\0", 8);
    g_app.m_dirtyDiskLoader = &loader;
    bool limited = false;
    try { AtFatal("screen", 0, 0, 0); } catch (const FrameLimit&) { limited = true; }
    CHECK(limited && owner.frames == 3);
    CHECK(g_atFatalScreenShown == 1 && g_launches == 0);
    CHECK(camera.m_owner == &owner && camera.m_matrix2C == &g_matrices[0] && camera.m_matrix30 == &g_matrices[1]);
    CHECK(camera.m_world == &g_matrices[2] && camera.m_local == &g_matrices[3]);
    const std::string frame =
        "begin 1 7 0;material 4;strip 4.0 1 [-0.5 -0.5 255.5 479.5] [0 0 256 480] 30;strip 4.1 1 [255.5 -0.5 511.5 479.5] [0 0 256 480] 30;"
        "material 5;strip 5.0 1 [511.5 -0.5 639.5 479.5] [0 0 128 480] 30;end;present 1;";
    CHECK(g_log == "images 0;camera;device;matrix;matrix;matrix;matrix;image 4;image 5;" + frame + frame + frame);
    if (g_fail) std::printf("%s\n", g_log.c_str());

    // A second fatal error, from inside the screen, goes straight out.
    g_log.clear();
    CHECK(run_fatal("again", 2) == 1);
    CHECK(g_log == "launch 1;print;exit 1;");

    // Each language its pair; an unknown one English.
    const char* names[5] = {"GERMAN", "ITALIAN", "ENGLISH", "", "ITALIANX"};
    const unsigned firsts[5] = {2, 6, 0, 0, 0};  // ITALIANX fills all eight characters, and differs from ITALIAN at the eighth
    for (int i = 0; i < 5; i++) {
        for (Image*& item : images.items) item = &left;
        reset();
        owner.frames = 2;
        App::s_instance = &g_app;
        g_app.m_dirtyDiskLoader = &loader;
        std::memset(g_app.m_settings.m_language, 0, 8);
        std::memcpy(g_app.m_settings.m_language, names[i], std::strlen(names[i]) < 8 ? std::strlen(names[i]) : 8);
        try { AtFatal("screen", 0, 0, 0); } catch (const FrameLimit&) {}
        char expect[40];
        std::snprintf(expect, sizeof expect, "image %u;image %u;", firsts[i], firsts[i] + 1);
        CHECK(g_log.find(expect) != std::string::npos);
    }
}

static bool near(int a, int b) { return a == b || a == b - 1; }

static void test_pieces() {
    CHECK(AtCompareName8("ABC", "ABC") == 0);
    CHECK(AtCompareName8("ABCDEFGHX", "ABCDEFGHY") == 0);  // eight at most
    CHECK(AtCompareName8("ABCDEFGX", "ABCDEFGY") == 'X' - 'Y');
    CHECK(AtCompareName8(nullptr, "") == 0);
    CHECK(AtCompareName8("A", nullptr) == 'A');
    CHECK(AtCompareName8("\xE9", "A") == (signed char)'\xE9' - 'A');  // signed characters

    AtRect clip = {0, 0, 100, 100}, rect = {-50, 10, 50, 110}, out, uv = {0, 0, 1, 1};
    CHECK(out.Clip(clip, rect, &uv) == 1);
    CHECK(out.left == 0 && out.top == 10 && out.right == 50 && out.bottom == 100);
    CHECK(uv.left == 0.5f && uv.top == 0 && uv.right == 1 && uv.bottom == 0.9f);
    AtRect away = {200, 0, 300, 50};
    uv = {0, 0, 1, 1};
    CHECK(out.Clip(clip, away, &uv) == 0 && uv.left == 0 && uv.right == 1);
    AtRect line = {10, 10, 20, 10};
    CHECK(out.Clip(clip, line, &uv) == 0);

    AtColor a = {0, 100, 200, 255}, b = {255, 0, 100, 255}, c;
    c.Lerp(a, b, 0.5f);
    CHECK(c.r == 127 && c.g == 50 && c.b == 150 && c.a == 255);
    AtColorF fa, fb, fc;
    fa.SetBytes(a);
    fb.SetBytes(b);
    fc.Lerp(fa, fb, 0.25f);
    CHECK(fc.r > 0.249f && fc.r < 0.251f && fc.a == 1.0f);
    c.LerpReal(fa, fb, 1.0f);  // through reals and back: a byte may come back one less, as the original truncates
    CHECK(near(c.r, 255) && c.g == 0 && near(c.b, 100));
    AtColorRectangle colors;
    colors.m_colors[0] = {0, 0, 0, 0};
    colors.m_colors[1] = {200, 0, 0, 0};
    colors.m_colors[2] = {0, 200, 0, 0};
    colors.m_colors[3] = {0, 0, 200, 0};
    AtRect full = {0, 0, 100, 100}, part = {50, 50, 100, 100};
    AtColorRectangle inner;
    inner.Clip(colors, full, part);
    CHECK(near(inner.m_colors[3].b, 200) && inner.m_colors[3].r == 0 && near(inner.m_colors[1].r, 100) && near(inner.m_colors[1].b, 100) && inner.m_colors[0].r < inner.m_colors[1].r);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // an unexpected stop ends the test, rather than waiting on a dialog
#endif
    std::printf("pieces\n");
    test_pieces();
    std::printf("way out\n");
    test_way_out();
    std::printf("rumble\n");
    test_rumble();
    std::printf("screen\n");
    test_screen();
    // The static fakes stand on memory that is not the game's heap (strips, entries, frames), and their recovered destructors would free it: leave without running them.
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        std::fflush(stdout);
        std::_Exit(1);
    }
    std::printf("fatal: all checks passed\n");
    std::fflush(stdout);
    std::_Exit(0);
}
