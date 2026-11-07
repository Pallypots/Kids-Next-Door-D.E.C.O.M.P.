# The scene format (.ASN): see include/at/AtScene.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtCoordSys.cpp
    ${KND_AT_SRC}/AtScene.cpp
    ${KND_AT_SRC}/AtSceneNode.cpp
    ${KND_AT_SRC}/AtSceneActorsHost.cpp)
knd_native_test(test_asn)
knd_oracle(oracle_asn)
