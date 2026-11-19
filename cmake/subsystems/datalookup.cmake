# Reading the design data once loaded: the registry's lookups, its copy form, start-up's calls, and the value classes' slots 4 to 9. See include/at/AtDataRegistry.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtDataLookup.cpp ${KND_AT_SRC}/AtDataValueAccess.cpp)
knd_native_test(test_datalookup)
knd_oracle(oracle_datalookup)
