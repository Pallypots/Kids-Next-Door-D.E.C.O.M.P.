# The sound notes (.GON): see include/gm/GmSoundNotes.h.
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmSoundNotes.cpp)
knd_native_test(test_gon)
knd_oracle(oracle_gon)
