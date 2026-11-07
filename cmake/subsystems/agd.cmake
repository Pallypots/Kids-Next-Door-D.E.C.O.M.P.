# The .AGD lists, and the .AGG, .AKA, .AGV and .AGP readers they reach: see include/at/AtDatabaseLoader.h, AtMeshDatabase.h and AtAnimationDatabase.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtDatabaseLoader.cpp
    ${KND_AT_SRC}/AtMeshDatabase.cpp
    ${KND_AT_SRC}/AtAnimationDatabase.cpp)
knd_native_test(test_agd)
knd_oracle(oracle_agd)
