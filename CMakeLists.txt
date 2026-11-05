# Native (portable) build of recovered source. This is NOT the matching/reference build — see tools/verify.py.
cmake_minimum_required(VERSION 3.20)
project(knd_recovered LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(knd_gm STATIC
    src/gm/GmLinkedListIterator.cpp
)
target_include_directories(knd_gm PUBLIC include)

enable_testing()
add_executable(test_gm_linkedlist tests/native/test_gm_linkedlist.cpp)
target_link_libraries(test_gm_linkedlist PRIVATE knd_gm)
add_test(NAME gm_linkedlist COMMAND test_gm_linkedlist)
