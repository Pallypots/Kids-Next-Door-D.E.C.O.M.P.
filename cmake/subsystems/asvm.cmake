# The Flash action script machine: its dispatch loop and handlers, and the values' and stack's methods only they use; see include/at/AtFlashScript.h. What it calls out to stops with a diagnostic
# (src/at/AtFpDisplaySeams.cpp); the C library's "%g" is src/at/AtFlashScriptHost.cpp.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtFlashScriptVM.cpp ${KND_AT_SRC}/AtFpDisplaySeams.cpp ${KND_AT_SRC}/AtFlashScriptHost.cpp)
knd_native_test(test_asvm)
knd_oracle(oracle_asvm)
