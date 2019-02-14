function(buildProject PROJECT_NAME)
    SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME})
    message(STATUS "Generating project file for example in ${PROJECT_FOLDER}")
    SET(SOURCE ${SOURCE} ${PROJECT_FOLDER}/*.cpp)
    SET(MAIN_CPP ${PROJECT_FOLDER}/${PROJECT_NAME}.cpp)
    add_executable(${PROJECT_NAME} ${MAIN_CPP})
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