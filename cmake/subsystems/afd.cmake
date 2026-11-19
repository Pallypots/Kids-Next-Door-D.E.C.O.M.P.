# The Flash user interface's .AFD reader, its sprites and frames, and its shapes: see include/at/AtFlashLevel.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFlashLevelRead.cpp ${KND_AT_SRC}/AtFlashSprite.cpp ${KND_AT_SRC}/AtFlashShape.cpp)
knd_native_test(test_afd)
knd_oracle(oracle_afd)
