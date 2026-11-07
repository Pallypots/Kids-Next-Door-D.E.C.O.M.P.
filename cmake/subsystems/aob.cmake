target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtAudioBank.cpp ${KND_AT_SRC}/AtAudioSegment.cpp)
knd_native_test(test_aob)
knd_oracle(oracle_aob)
