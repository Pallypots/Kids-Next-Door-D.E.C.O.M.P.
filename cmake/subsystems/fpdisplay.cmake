# The Flash player's display manager: its contexts, levels, sprite nodes' frames, labels, paths, properties and display lists, sprites' variables and the remote function calls; see include/at/AtFpDisplay.h.
# What it calls that loads, plays, draws or runs a level's frames stops with a diagnostic (src/at/AtFpDisplaySeams.cpp, built with asvm).
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFpDisplay.cpp)
knd_native_test(test_fpdisplay)
knd_oracle(oracle_fpdisplay)
