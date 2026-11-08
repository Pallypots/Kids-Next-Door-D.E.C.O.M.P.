# The data library, .ADD, and the database manager, .ADB: see include/at/AtDataLibrary.h. AtDataLibraryHost.cpp holds the seams to the .ADR classes.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtDataLibrary.cpp ${KND_AT_SRC}/AtDatabaseManager.cpp ${KND_AT_SRC}/AtDataLibraryHost.cpp)
knd_native_test(test_add)
knd_oracle(oracle_add)
