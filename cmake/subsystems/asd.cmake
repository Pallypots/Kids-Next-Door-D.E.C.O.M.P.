# Scene lists (.ASD), cell graphs (.ASC), flat images (.ASI) and text (.AST): see include/at/AtSceneDatabaseLoader.h.
target_sources(knd_at PRIVATE
    ${KND_AT_SRC}/AtCellGraph.cpp
    ${KND_AT_SRC}/AtDrawItem.cpp
    ${KND_AT_SRC}/AtSceneDatabaseLoader.cpp)
knd_native_test(test_asd)
knd_oracle(oracle_asd)
