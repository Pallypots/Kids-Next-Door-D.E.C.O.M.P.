# The game's fatal error path: the report of a failed file, AtFatal, the rumble it stops and the error screen it draws. See include/at/AtFatal.h for how it meets platform/fatal.cpp.
# AtFatal.cpp holds the game's AtFatal alone, so that a program with its own (platform/fatal.cpp's stand-in, a test's, an oracle's) never pulls it in.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFatal.cpp ${KND_AT_SRC}/AtReport.cpp ${KND_AT_SRC}/AtReportHost.cpp ${KND_AT_SRC}/AtControllerManager.cpp ${KND_AT_SRC}/AtGr2D.cpp)
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmRumblePlayback.cpp ${KND_GM_SRC}/GmControllerModule.cpp ${KND_GM_SRC}/GmFatalScreen.cpp)
# Not knd_native_test: the test links the game's AtFatal, with the App it draws on (GmMain.cpp), and supplies the port's functions itself, so that the stop can be caught.
add_executable(test_fatal tests/native/test_fatal.cpp ${KND_GM_SRC}/GmMain.cpp)
target_link_libraries(test_fatal PRIVATE knd_at knd_gm)
add_test(NAME test_fatal COMMAND test_fatal)
knd_oracle(oracle_fatal)
if(TARGET oracle_fatal)
    target_sources(oracle_fatal PRIVATE ${KND_GM_SRC}/GmMain.cpp)
endif()
