set (SOURCE blending.cpp)
set (COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set (COMMON_SRC ${COMMON}/BaseApp.cpp
                ${COMMON}/BaseTimer.cpp
                ${COMMON}/DDSTextureLoader.cpp
                ${COMMON}/MathHelper.cpp)
add_executable(blending ${SOURCE} ${COMMON_SRC})
