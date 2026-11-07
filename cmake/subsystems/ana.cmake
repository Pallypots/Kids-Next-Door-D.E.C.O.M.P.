# Input mappers, the .ANM format: see include/at/AtInputMapper.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtInputMapper.cpp ${KND_AT_SRC}/AtInputRouter.cpp)
knd_native_test(test_ana)
knd_oracle(oracle_ana)
