set (SOURCE compute_shader.cpp)
set (COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set (COMMON_SRC ${COMMON}/BaseApp.cpp
                ${COMMON}/BaseTimer.cpp
                ${COMMON}/DDSTextureLoader.cpp
                ${COMMON}/MathHelper.cpp)
add_executable(compute_shader ${SOURCE} ${COMMON_SRC})
