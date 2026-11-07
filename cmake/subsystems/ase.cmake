# Particle emitters, the .ASE format: see include/at/AtEmitter.h and include/at/AtSegmentQuery.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtEmitter.cpp ${KND_AT_SRC}/AtSegmentQuery.cpp)
knd_native_test(test_ase)
knd_oracle(oracle_ase)
