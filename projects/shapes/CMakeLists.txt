set(SOURCE shapes.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp 
               ${COMMON}/BaseTimer.cpp
               ${COMMON}/MathHelper.cpp)

add_executable(shapesDemo ${SOURCE} ${COMMON_SRC})