# What a 2D texture does when it loads, short of the device: see include/at/AtGrD3DTextureLoad.h. The device's calls are in AtGrD3DTextureSeam.cpp (xdb.cmake), which a test supplies.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtGrIPixelMap.cpp
    ${KND_AT_SRC}/AtGrD3DTextureLoad.cpp)
knd_native_test(test_texload)
knd_oracle(oracle_texload)
