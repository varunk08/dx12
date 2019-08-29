cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)

project(vkd3d12)
include_directories(lib)
include_directories(projects/common/)
add_definitions(-DUNICODE)
add_subdirectory(projects)