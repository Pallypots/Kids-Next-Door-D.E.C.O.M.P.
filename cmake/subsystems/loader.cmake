# The Xbox database loader and the binary meshes and animations it reaches: see include/at/AtGrD3DDatabase.h. AtGrD3DUnrecovered.cpp and AtGrD3DSeam.cpp are not recovered code; a test supplies the seam's functions.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtGrD3DDatabaseLoader.cpp
    ${KND_AT_SRC}/AtGrD3DMeshDatabase.cpp
    ${KND_AT_SRC}/AtGrD3DAnimation.cpp
    ${KND_AT_SRC}/AtAnimationKeys.cpp
    ${KND_AT_SRC}/AtGrD3DUnrecovered.cpp
    ${KND_AT_SRC}/AtGrD3DSeam.cpp)
knd_native_test(test_loader)
knd_oracle(oracle_loader)
