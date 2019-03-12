set(SOURCE drawBox.cpp)
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_SRC ${COMMON}/BaseApp.cpp 
               ${COMMON}/BaseTimer.cpp)

add_executable(drawBox ${SOURCE} ${COMMON_SRC})