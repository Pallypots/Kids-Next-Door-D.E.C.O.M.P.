# The Flash user interface's text formats and level files, .AFX and .AFL: see include/at/AtFlashTextFormat.h and include/at/AtFlashLevel.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFlashTextFormat.cpp ${KND_AT_SRC}/AtFlashLevel.cpp)
knd_native_test(test_afx)
knd_oracle(oracle_afx)
