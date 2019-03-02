set(SOURCE d3d_initialization.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp 
               ${COMMON}/BaseTimer.cpp)

add_executable(d3d_initialization ${SOURCE} ${COMMON_SRC})