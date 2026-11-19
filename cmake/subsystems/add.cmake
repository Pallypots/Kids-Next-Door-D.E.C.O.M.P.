# The data library, .ADD, and the database manager, .ADB: see include/at/AtDataLibrary.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtDataLibrary.cpp ${KND_AT_SRC}/AtDatabaseManager.cpp)
knd_native_test(test_add)
knd_oracle(oracle_add)
