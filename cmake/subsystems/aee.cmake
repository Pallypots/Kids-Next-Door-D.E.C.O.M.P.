# Events by name, .AEE files: the reader, an event's subscriptions and its publishing. See include/at/AtEvent.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtEvent.cpp ${KND_AT_SRC}/AtEventLoad.cpp)
knd_native_test(test_aee)
knd_oracle(oracle_aee)
