# Controllers, the .ANA format, and binding mappers to them: see include/at/AtInputController.h. Binding itself is in AtInputRouter.cpp and AtInputMapper.cpp, part of ana.cmake.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtInputController.cpp)
knd_native_test(test_anaf)
knd_oracle(oracle_anaf)
