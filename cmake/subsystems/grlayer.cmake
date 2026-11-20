# The graphics layer's object short of the hardware (at/grlayer). See include/at/AtGraphicsLayer.h. AtGraphicsLayerSeams.cpp holds what it reaches and is
# not recovered, each stopping with a diagnostic.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtGraphicsLayer.cpp ${KND_AT_SRC}/AtGraphicsLayerHeaps.cpp ${KND_AT_SRC}/AtGraphicsLayerSeams.cpp)
# Not knd_native_test's seams: the test supplies the layer's seams itself (a logged stand-in factory), so the seams file is not linked.
add_executable(test_grlayer tests/native/test_grlayer.cpp platform/fatal.cpp)
target_link_libraries(test_grlayer PRIVATE knd_at knd_gm)
add_test(NAME test_grlayer COMMAND test_grlayer)
knd_oracle(oracle_grlayer)
