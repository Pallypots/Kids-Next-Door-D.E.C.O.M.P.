# FaCmCombat's update (slot 6) and what it alone reaches in its own range, with its seams: see include/gm/FaCmCombat.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/FaCmCombatUpdate.cpp
    ${KND_GM_SRC}/CombatNames.cpp
    ${KND_GM_SRC}/CombatSeams.cpp)
knd_native_test(test_combat)
knd_oracle(oracle_combat)
