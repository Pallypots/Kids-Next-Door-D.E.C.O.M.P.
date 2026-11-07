# The .AOD and .AOE files and the Xbox sound classes: see include/at/AtAudioDatabase.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtAudioDatabase.cpp
    ${KND_AT_SRC}/AtSoXbox.cpp
    ${KND_AT_SRC}/AtSoXboxHost.cpp)
knd_native_test(test_aod)
knd_oracle(oracle_aod)
