# The group, water, lens flare and sound classes of a scene node, and the walks that put a scene's nodes into a space graph: see include/at/AtSceneNodes.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtSceneEffectNodes.cpp
    ${KND_AT_SRC}/AtSceneGroup.cpp
    ${KND_AT_SRC}/AtSceneLoaderWalk.cpp
   
    ${KND_AT_SRC}/AtSceneNodesHost.cpp
    ${KND_AT_SRC}/AtSceneWalk.cpp)
knd_native_test(test_scenenodes)
knd_oracle(oracle_scenenodes)
