# The graphics factory's start: the render function tables, the factory, its adapter list and what Direct3D reports of each adapter (at/grfactory). See
# include/at/AtGraphicsFactory.h. AtGraphicsFactorySeams.cpp holds what the start reaches and is not recovered; AtGraphicsFactoryD3d.cpp the SDK's
# Direct3D entry points, which stop unless a program answers them (platform/host_d3d.cpp, port code, does on the host).
target_sources(knd_at PRIVATE ${KND_AT_SRC}/AtGraphicsFactory.cpp ${KND_AT_SRC}/AtGraphicsFactoryStart.cpp ${KND_AT_SRC}/AtGraphicsFactorySlots.cpp ${KND_AT_SRC}/AtGraphicsFactorySeams.cpp ${KND_AT_SRC}/AtGraphicsFactoryD3d.cpp)
if(TARGET test_app)
    target_sources(test_app PRIVATE platform/host_d3d.cpp)
endif()
# The test answers Direct3D itself, with doubles.
knd_native_test(test_grfactory)
knd_oracle(oracle_grfactory)
