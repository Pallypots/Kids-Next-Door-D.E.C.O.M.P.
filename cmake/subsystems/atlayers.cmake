# The math, data management, script and kinematics layers' objects (at/atlayers). See include/at/AtApMidLayers.h. AtApMidLayersSeams.cpp holds what
# they reach and is not recovered, each stopping with a diagnostic.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtApMidLayers.cpp ${KND_AT_SRC}/AtApMidLayersSeams.cpp)
knd_native_test(test_atlayers)
knd_oracle(oracle_atlayers)
