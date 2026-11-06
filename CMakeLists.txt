# Native (portable) build of recovered source. This is NOT the matching/reference build — see tools/verify.py.
cmake_minimum_required(VERSION 3.20)
project(knd_recovered LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# KND_GM_SRC exists for tools/oracle_run.py, which builds deliberately broken copies as negative controls.
set(KND_GM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/gm" CACHE PATH "Directory of the gm sources")
add_library(knd_gm STATIC
    ${KND_GM_SRC}/GmConnection.cpp
    ${KND_GM_SRC}/GmAppSettings.cpp
    ${KND_GM_SRC}/GmAudioData.cpp
    ${KND_GM_SRC}/GmManifestLoader.cpp
    ${KND_GM_SRC}/GmLevelManager.cpp
    ${KND_GM_SRC}/GmLevelPending.cpp
    ${KND_GM_SRC}/GmLinkedList.cpp
    ${KND_GM_SRC}/GmNameMap.cpp
    ${KND_GM_SRC}/GmRumble.cpp
    ${KND_GM_SRC}/GmSocket.cpp
)
target_include_directories(knd_gm PUBLIC include)

# KND_AT_SRC: as KND_GM_SRC.
set(KND_AT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/at" CACHE PATH "Directory of the at sources")
add_library(knd_at STATIC
    ${KND_AT_SRC}/AtHeap.cpp
    ${KND_AT_SRC}/AtIoFastAlignedJamHeader.cpp
    ${KND_AT_SRC}/AtIoFastJamHeader.cpp
    ${KND_AT_SRC}/AtIoFileSystem.cpp
    ${KND_AT_SRC}/AtIoFormatHost.cpp
    ${KND_AT_SRC}/AtIoJamFileSystem.cpp
    ${KND_AT_SRC}/AtIoJamFile.cpp
    ${KND_AT_SRC}/AtIoJamNameTable.cpp
    ${KND_AT_SRC}/AtIoJamPending.cpp
    ${KND_AT_SRC}/AtIoMountManager.cpp
    ${KND_AT_SRC}/AtText.cpp
    ${KND_AT_SRC}/AtIoStream.cpp
    ${KND_AT_SRC}/AtIoTextTokenizer.cpp
    ${KND_AT_SRC}/AtIoTokenizer.cpp
)
target_include_directories(knd_at PUBLIC include)
# The two depend on each other: the context owns a namespace database, and readers in gm use the tokenizer.
target_link_libraries(knd_at PUBLIC knd_gm)
target_link_libraries(knd_gm PUBLIC knd_at)

enable_testing()
add_executable(test_gm_linkedlist tests/native/test_gm_linkedlist.cpp)
target_link_libraries(test_gm_linkedlist PRIVATE knd_gm)
add_test(NAME gm_linkedlist COMMAND test_gm_linkedlist)
add_executable(test_at_jam tests/native/test_at_jam.cpp platform/fatal.cpp)
target_link_libraries(test_at_jam PRIVATE knd_at)
add_test(NAME at_jam COMMAND test_at_jam)
add_executable(test_at_e2e tests/native/test_at_e2e.cpp platform/fatal.cpp platform/host_file_system.cpp)
target_include_directories(test_at_e2e PRIVATE platform)
target_link_libraries(test_at_e2e PRIVATE knd_at)
add_test(NAME at_e2e COMMAND test_at_e2e)
add_executable(test_at_tok tests/native/test_at_tok.cpp)
target_include_directories(test_at_tok PRIVATE tests/oracle)
target_link_libraries(test_at_tok PRIVATE knd_at)
add_test(NAME at_tok COMMAND test_at_tok)
add_executable(test_at_heap tests/native/test_at_heap.cpp platform/fatal.cpp)
target_link_libraries(test_at_heap PRIVATE knd_at knd_gm)
add_test(NAME at_heap COMMAND test_at_heap)
add_executable(test_gm_levels tests/native/test_gm_levels.cpp platform/fatal.cpp platform/host_file_system.cpp)
target_include_directories(test_gm_levels PRIVATE platform)
target_link_libraries(test_gm_levels PRIVATE knd_gm knd_at)
add_test(NAME gm_levels COMMAND test_gm_levels)

# Local-only differential oracle: 32-bit Windows, runs original code from the owner's XBE beside recovered source.
# Never part of a release. Build with: cmake -S . -B build/oracle -A Win32
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    enable_language(ASM_MASM)
    add_executable(oracle_gm tests/oracle/reserve.asm tests/oracle/oracle_gm.cpp)
    target_link_libraries(oracle_gm PRIVATE knd_gm)
    add_executable(oracle_jam tests/oracle/reserve.asm tests/oracle/oracle_jam.cpp)
    target_link_libraries(oracle_jam PRIVATE knd_at)
    add_executable(oracle_lvl tests/oracle/reserve.asm tests/oracle/oracle_lvl.cpp)
    target_link_libraries(oracle_lvl PRIVATE knd_gm knd_at)
    target_link_options(oracle_lvl PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_mnt tests/oracle/reserve.asm tests/oracle/oracle_mnt.cpp)
    target_link_libraries(oracle_mnt PRIVATE knd_at)
    target_link_options(oracle_mnt PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_rmb tests/oracle/reserve.asm tests/oracle/oracle_rmb.cpp)
    target_link_libraries(oracle_rmb PRIVATE knd_gm knd_at)
    target_link_options(oracle_rmb PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_txt tests/oracle/reserve.asm tests/oracle/oracle_txt.cpp)
    target_link_libraries(oracle_txt PRIVATE knd_at knd_gm)
    target_link_options(oracle_txt PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_set tests/oracle/reserve.asm tests/oracle/oracle_set.cpp)
    target_link_libraries(oracle_set PRIVATE knd_gm knd_at)
    target_link_options(oracle_set PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_hep tests/oracle/reserve.asm tests/oracle/oracle_hep.cpp)
    target_link_libraries(oracle_hep PRIVATE knd_at knd_gm)
    target_link_options(oracle_hep PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_ldr tests/oracle/reserve.asm tests/oracle/oracle_ldr.cpp)
    target_link_libraries(oracle_ldr PRIVATE knd_gm knd_at)
    target_link_options(oracle_ldr PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_aud tests/oracle/reserve.asm tests/oracle/oracle_aud.cpp)
    target_link_libraries(oracle_aud PRIVATE knd_gm knd_at)
    target_link_options(oracle_aud PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    add_executable(oracle_tok tests/oracle/reserve.asm tests/oracle/oracle_tok.cpp)
    target_link_libraries(oracle_tok PRIVATE knd_at)
    target_link_options(oracle_tok PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    target_link_options(oracle_jam PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
    # Base 0x10000: tests/oracle/reserve.asm owns the XBE's address range from process creation.
    target_link_options(oracle_gm PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.text,ERW /INCREMENTAL:NO)
endif()
