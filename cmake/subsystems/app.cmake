# App: the game's main, start-up and main loop. See include/gm/GmApp.h. GmAppHost.cpp holds the seams that stop with a diagnostic. GmMain.cpp holds main and
# the App object, so only programs link it, never the library.
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmApp.cpp ${KND_GM_SRC}/GmAppIo.cpp ${KND_GM_SRC}/GmAppHost.cpp ${KND_GM_SRC}/GmDataRoot.cpp)
# Not knd_native_test: the test supplies platform/fatal.cpp's functions itself, so that a stop can be caught.
add_executable(test_app tests/native/test_app.cpp ${KND_GM_SRC}/GmMain.cpp platform/host_file_system.cpp)
target_include_directories(test_app PRIVATE platform)
target_link_libraries(test_app PRIVATE knd_at knd_gm)
add_test(NAME test_app COMMAND test_app)
knd_oracle(oracle_app)
if(TARGET oracle_app)
    target_sources(oracle_app PRIVATE ${KND_GM_SRC}/GmMain.cpp)
endif()
