set(SOURCE shapes_shaded.cpp
           FrameResource.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp 
               ${COMMON}/BaseTimer.cpp
               ${COMMON}/MathHelper.cpp
               ${COMMON}/GeometryGenerator.cpp)

add_executable(shapesShaded ${SOURCE} ${COMMON_SRC})