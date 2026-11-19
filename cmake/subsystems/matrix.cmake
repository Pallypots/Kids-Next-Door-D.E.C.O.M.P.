# The matrix table's functions: see include/at/AtMatrix.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtMatrix.cpp)
knd_native_test(test_matrix)
target_include_directories(test_matrix PRIVATE tests/oracle)
knd_oracle(oracle_matrix)
