# The Xbox shader, light, font and image databases the concrete loader makes: see include/at/AtGrD3DItems.h. AtGrD3DItemSeam.cpp is not recovered code; a test supplies its functions.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtGrD3DShaderDatabase.cpp
    ${KND_AT_SRC}/AtGrD3DFontImage.cpp
    ${KND_AT_SRC}/AtGrD3DItemSeam.cpp)
knd_oracle(oracle_xshade)
knd_native_test(test_xshade)
