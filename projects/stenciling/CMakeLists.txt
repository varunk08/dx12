set (SOURCE stenciling.cpp)
set (COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set (COMMON_SRC ${COMMON}/BaseApp.cpp
                ${COMMON}/MathHelper.cpp
                ${COMMON}/BaseTimer.cpp
                ${COMMON}/GeometryGenerator.cpp)

add_executable(stenciling ${SOURCE} ${COMMON_SRC})
