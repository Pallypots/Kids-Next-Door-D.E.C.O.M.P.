# The behaviour scripts' abstract machine (a Small/AMX interpreter), the script objects and the script system's loading and running (at/smallvm). See
# include/at/AtSmallVm.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtSmallVm.cpp ${KND_AT_SRC}/AtSmallCScripts.cpp ${KND_AT_SRC}/AtSmallCScriptOwner.cpp)
knd_native_test(test_smallvm)
knd_oracle(oracle_smallvm)
