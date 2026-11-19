# The level manifest's base class: its words, Finished and seven kinds of item. See include/gm/GmDatabaseManifestItems.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmDatabaseManifest.cpp
    ${KND_GM_SRC}/GmDatabaseManifestAsd.cpp
    ${KND_GM_SRC}/GmDatabaseManifestHost.cpp)
knd_native_test(test_dbmanifest)
knd_oracle(oracle_dbmanifest)
