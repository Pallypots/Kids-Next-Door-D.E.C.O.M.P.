target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtMaterialOwnerLists.cpp ${KND_AT_SRC}/AtMatAssignment.cpp ${KND_AT_SRC}/AtMatAnimation.cpp ${KND_AT_SRC}/AtImageDatabase.cpp ${KND_AT_SRC}/AtLight.cpp ${KND_AT_SRC}/AtLensFlare.cpp)
knd_native_test(test_agn)
knd_oracle(oracle_agn)
