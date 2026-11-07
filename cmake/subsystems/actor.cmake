# The actor base, ModelActor and JointedModelActor of a scene: see include/at/AtSceneActor.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtSceneActor.cpp
    ${KND_AT_SRC}/AtSceneAnimation.cpp
    ${KND_AT_SRC}/AtSceneModelActor.cpp)
knd_native_test(test_actor)
knd_oracle(oracle_actor)
