# Playing sound notes, the voice slots, sound nodes (.GOO), collision (.GOC) and instance (.GOI) sounds, and the game's random numbers: see include/gm/GmSoundNodes.h, include/gm/GmAudioData.h
# and include/gm/GmRandom.h. What the sound manager needs of the Atlas classes is in src/at/AtAudioVoices.cpp.
target_sources(knd_gm PRIVATE ${KND_GM_SRC}/GmSoundPlay.cpp ${KND_GM_SRC}/GmSoundNodes.cpp ${KND_GM_SRC}/GmRandom.cpp ${KND_GM_SRC}/GmAppStatics.cpp)
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtAudioVoices.cpp)
knd_native_test(test_gmsound)
knd_oracle(oracle_gmsound)
