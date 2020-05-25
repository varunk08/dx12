set (SOURCE InstancingCulling.cpp)
set (COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set (COMMON_SRC ${COMMON}/BaseApp.cpp
                ${COMMON}/BaseTimer.cpp
                ${COMMON}/DDSTextureLoader.cpp
                ${COMMON}/MathHelper.cpp
                ${COMMON}/GeometryGenerator.cpp
                ${COMMON}/BaseTimer.cpp)
add_executable(instancing_culling ${SOURCE} ${COMMON_SRC})
