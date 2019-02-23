set(SOURCE d3d_initialization.cpp)
SET(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
add_executable(d3d_initialization ${SOURCE} ${COMMON}/BaseApp.cpp)