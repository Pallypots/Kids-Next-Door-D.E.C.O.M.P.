# The game layer's actor base, GmAcActor, with GmAcNode, GmBaObject and the animation callback: see include/gm/GmAcActor.h.
target_sources(knd_gm PRIVATE
    ${KND_GM_SRC}/GmAcNode.cpp
    ${KND_GM_SRC}/GmAcActor.cpp
    ${KND_GM_SRC}/GmAcSeams.cpp)
knd_native_test(test_gmactor)
knd_oracle(oracle_gmactor)
