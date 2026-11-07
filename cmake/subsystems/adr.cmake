# The design data registry, the .ADR format: see include/at/AtDataRegistry.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtDataRegistry.cpp ${KND_AT_SRC}/AtDataComponent.cpp)
knd_native_test(test_adr)
knd_oracle(oracle_adr)
