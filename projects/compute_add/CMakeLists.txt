set(SOURCE compute_add.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp 
               ${COMMON}/BaseTimer.cpp)

add_executable(compute_add ${SOURCE} ${COMMON_SRC})