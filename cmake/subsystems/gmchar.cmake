# GmAcObject and GmAcCharacter, and the design record reader every actor shares: see include/gm/GmAcCharacter.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmAcRecords.cpp
    ${KND_GM_SRC}/GmAcObject.cpp
    ${KND_GM_SRC}/GmAcCharacter.cpp
    ${KND_GM_SRC}/GmAcCharSeams.cpp)
knd_native_test(test_gmchar)
knd_oracle(oracle_gmchar)
