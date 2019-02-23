function(buildProject PROJECT_NAME)
    SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME})
    #include_directories(${PROJECT_FOLDER})
    #message(STATUS "Generating project file for example in ${PROJECT_FOLDER}")
    #include_directories(PUBLIC ${PROJECT_FOLDER}/../common)
    #SET(SOURCE ${SOURCE} ${PROJECT_FOLDER}/*.cpp )
    #SET(MAIN_CPP ${PROJECT_FOLDER}/${PROJECT_NAME}.cpp)
    #add_executable(${PROJECT_NAME} ${MAIN_CPP} ${COMMON})
    add_subdirectory(${PROJECT_FOLDER})
endfunction(buildProject)

function(buildAllProjects)
    foreach(PROJECT ${PROJECTS})
        buildProject(${PROJECT})
    endforeach(PROJECT)
endfunction(buildAllProjects)

set (PROJECTS 
     basic_window
     hello_world
     d3d_initialization)

buildAllProjects()