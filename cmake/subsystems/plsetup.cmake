# The player's set-up, FaAcPlayer's slot 15, and the combat modules GmCmCombat, FaCmCombat and FaCmPlayerCombat: see include/gm/FaCmCombat.h and include/gm/GmAcPlayer.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/FaAcPlayerSetup.cpp
    ${KND_GM_SRC}/GmCmCombat.cpp
    ${KND_GM_SRC}/PlSetupSeams.cpp)
knd_native_test(test_plsetup)
knd_oracle(oracle_plsetup)
