# The matrix device and its pool, and the last five matrix-table slots the game reads: see include/at/AtMatrixDevice.h and include/at/AtMatrix.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtMatrixDevice.cpp ${KND_AT_SRC}/AtMatrixAxis.cpp)
knd_native_test(test_matdev)
target_include_directories(test_matdev PRIVATE tests/oracle)
knd_oracle(oracle_matdev)
