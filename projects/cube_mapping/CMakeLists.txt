set (SOURCE CubeMapping.cpp)
set (COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set (COMMON_SRC ${COMMON}/BaseApp.cpp
                ${COMMON}/BaseTimer.cpp
                ${COMMON}/DDSTextureLoader.cpp
                ${COMMON}/MathHelper.cpp
                ${COMMON}/GeometryGenerator.cpp
                ${COMMON}/BaseTimer.cpp)
add_executable(cube_mapping ${SOURCE} ${COMMON_SRC})
