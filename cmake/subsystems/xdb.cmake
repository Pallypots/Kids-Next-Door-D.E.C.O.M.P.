# The Xbox texture and material databases the concrete loader makes: see include/at/AtGrD3DTextures.h. AtGrD3DTextureSeam.cpp is not recovered code; a test supplies its functions.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtGrD3DTextureDatabase.cpp
    ${KND_AT_SRC}/AtGrD3DMaterialDatabase.cpp
    ${KND_AT_SRC}/AtGrD3DTextureSeam.cpp)
knd_native_test(test_xdb)
knd_oracle(oracle_xdb)
