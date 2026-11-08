# The level manifest, the .MNG files' parser and items: see include/gm/GmLevelManifest.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmLevelManifest.cpp
    ${KND_GM_SRC}/GmLevelManifestItems.cpp
    ${KND_GM_SRC}/GmLevelManifestHost.cpp
    ${KND_GM_SRC}/GmLevelManifestNotes.cpp)
knd_native_test(test_manifest)
knd_oracle(oracle_manifest)
