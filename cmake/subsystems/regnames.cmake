# The registry names: loaded design data registries by the name of their file. See include/gm/GmRegistryNames.h.
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmRegistryNames.cpp ${KND_GM_SRC}/GmRegistryNamesAdd.cpp)
knd_native_test(test_regnames)
knd_oracle(oracle_regnames)
