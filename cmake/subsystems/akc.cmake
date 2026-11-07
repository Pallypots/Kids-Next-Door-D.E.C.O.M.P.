# Skeletons (.AKC), and the base of every texture, whose reader the .AGT files reach: see include/at/AtCoordSys.h and include/at/AtTextureItem.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtCoordSysFile.cpp
    ${KND_AT_SRC}/AtTextureItem.cpp)
knd_native_test(test_akc)
knd_oracle(oracle_akc)
