# The playable characters, GmAcPlayer and FaAcPlayer, and the controller frame: see include/gm/GmAcPlayer.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmAcPlayer.cpp
    ${KND_GM_SRC}/FaAcPlayer.cpp
    ${KND_GM_SRC}/GmAcPlayerSeams.cpp)
knd_native_test(test_gmplayer)
knd_oracle(oracle_gmplayer)
