# The AtAp layers' start-ups below the atlas layer, App's configs for them, and the event, network and I/O layers' objects. See include/at/AtApLayer.h and
# include/gm/GmApp.h. AtIoSystemDevices.cpp stops where the Xbox's I/O device and file system would be made, and GmAppSdk.cpp stops for the SDK's memory
# query: a program that links platform/host_io.cpp and platform/host_sdk.cpp (port code) gets the host's answers instead.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtApGlobals.cpp ${KND_AT_SRC}/AtApLayers.cpp ${KND_AT_SRC}/AtIoSystemFactory.cpp ${KND_AT_SRC}/AtIoSystemDevices.cpp)
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmAppLayers.cpp ${KND_GM_SRC}/GmAppSdk.cpp)
# The real start-up, on the host, answers the SDK and makes the host's file systems.
if(TARGET test_app)
    target_sources(test_app PRIVATE platform/host_sdk.cpp platform/host_io.cpp)
endif()
# Not knd_native_test: like test_app, it supplies platform/fatal.cpp's functions itself, so that a stop can be caught.
add_executable(test_atap tests/native/test_atap.cpp platform/host_file_system.cpp platform/host_io.cpp)
target_include_directories(test_atap PRIVATE platform)
target_link_libraries(test_atap PRIVATE knd_at knd_gm)
add_test(NAME test_atap COMMAND test_atap)
knd_oracle(oracle_atap)
