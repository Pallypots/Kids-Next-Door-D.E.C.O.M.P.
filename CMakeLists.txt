# Native (portable) build of recovered source. This is NOT the matching/reference build — see tools/verify.py.
cmake_minimum_required(VERSION 3.20)
project(knd_recovered LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# KND_GM_SRC exists for tools/oracle_run.py, which builds deliberately broken copies as negative controls.
set(KND_GM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/gm" CACHE PATH "Directory of the gm sources")
add_library(knd_gm STATIC
    ${KND_GM_SRC}/GmConnection.cpp
    ${KND_GM_SRC}/GmLinkedList.cpp
    ${KND_GM_SRC}/GmSocket.cpp
)
target_include_directories(knd_gm PUBLIC include)

enable_testing()
add_executable(test_gm_linkedlist tests/native/test_gm_linkedlist.cpp)
target_link_libraries(test_gm_linkedlist PRIVATE knd_gm)
add_test(NAME gm_linkedlist COMMAND test_gm_linkedlist)

# Local-only differential oracle: 32-bit Windows, runs original code from the owner's XBE beside recovered source.
# Never part of a release. Build with: cmake -S . -B build/oracle -A Win32
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_executable(oracle_gm tests/oracle/oracle_gm.cpp)
    target_link_libraries(oracle_gm PRIVATE knd_gm)
    # Base 0x10000: section .aorig (see the source) owns the XBE's address range from process creation.
    target_link_options(oracle_gm PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /SAFESEH:NO /SECTION:.aorig,ERW)
endif()
