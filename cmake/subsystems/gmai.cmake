# The AI action manager that enemies use, the behaviour chart's commands that drive it, and the game's random numbers: see include/gm/GmAiActionManager.h and include/gm/GmRandom.h (gm/gmsound's).
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmAiActionManager.cpp
    ${KND_GM_SRC}/GmAiChart.cpp
    ${KND_GM_SRC}/GmAiSeams.cpp)
knd_native_test(test_gmai)
knd_oracle(oracle_gmai)
