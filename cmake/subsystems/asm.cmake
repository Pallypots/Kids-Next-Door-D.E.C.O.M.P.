# Modifiers (.ASM) and space graphs (.ASG): see include/at/AtModifier.h and include/at/AtSpaceGraph.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtModifier.cpp
    ${KND_AT_SRC}/AtModifierState.cpp
    ${KND_AT_SRC}/AtSpaceGraph.cpp
    ${KND_AT_SRC}/AtSpace.cpp)
knd_native_test(test_asm)
knd_oracle(oracle_asm)
