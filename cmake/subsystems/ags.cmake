target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtShader.cpp ${KND_AT_SRC}/AtShaderDatabase.cpp)
knd_native_test(test_ags)
knd_oracle(oracle_ags)
