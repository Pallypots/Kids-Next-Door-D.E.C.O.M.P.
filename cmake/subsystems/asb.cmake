# Bounding geometry, the .ASB format: see include/at/AtBoundingGeometry.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtBoundingBox.cpp
    ${KND_AT_SRC}/AtBoundingDraw.cpp
    ${KND_AT_SRC}/AtBoundingGeometry.cpp
    ${KND_AT_SRC}/AtBoundingGeometryHost.cpp)
knd_native_test(test_asb)
knd_oracle(oracle_asb)
