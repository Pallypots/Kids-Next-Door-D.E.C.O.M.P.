# A sound emitter's voices, the voice classes, and the Xbox bank's play slots up to XACT: see include/at/AtAudioDatabase.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtAudioEmitterPlay.cpp
    ${KND_AT_SRC}/AtSoXboxPlay.cpp
    ${KND_AT_SRC}/AtSoXboxXactHost.cpp)
knd_native_test(test_emitplay)
knd_oracle(oracle_emitplay)
