# The Flash user interface's texts (.AFT, with their fonts and formats) and scripts (.AFA, with the action script machine's state): see include/at/AtFlashText.h and include/at/AtFlashScript.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFlashText.cpp ${KND_AT_SRC}/AtFlashScript.cpp)
knd_native_test(test_aft)
knd_oracle(oracle_aft)
