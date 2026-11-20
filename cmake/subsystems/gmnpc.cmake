# GmAcNPC and FaAcNPC, the enemies and the game's other AI characters: see include/gm/GmAcNPC.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmAcNPC.cpp
    ${KND_GM_SRC}/FaAcNPC.cpp
    ${KND_GM_SRC}/GmAcNPCSeams.cpp)
knd_native_test(test_gmnpc)
knd_oracle(oracle_gmnpc)
