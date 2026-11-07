# ParticleSystemActor and the table of random words: see include/at/AtSceneParticle.h and include/at/AtRandom.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtRandom.cpp
    ${KND_AT_SRC}/AtSceneParticle.cpp)
knd_native_test(test_particle)
knd_oracle(oracle_particle)
