set(SOURCE Picking.cpp
           FrameResource.cpp
           Camera.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp
               ${COMMON}/BaseTimer.cpp
               ${COMMON}/DDSTextureLoader.cpp
               ${COMMON}/MathHelper.cpp
               ${COMMON}/GeometryGenerator.cpp)

add_executable(picking ${SOURCE} ${COMMON_SRC})