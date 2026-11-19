# The graphics device: its abstract layers, the concrete device and its members' set-up. See include/at/AtGraphicsDevice.h, include/at/AtMaterial.h and include/at/AtDisplayDevice.h.
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtMaterialOwnerBase.cpp ${KND_AT_SRC}/AtIndexArray.cpp ${KND_AT_SRC}/AtGraphicsDevice.cpp ${KND_AT_SRC}/AtGraphicsDeviceHost.cpp)
knd_native_test(test_gfxdev)
knd_oracle(oracle_gfxdev)
