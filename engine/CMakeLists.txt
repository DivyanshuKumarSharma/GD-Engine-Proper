cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(Engine)

set(PROJECT_SOURCE_FILES
	source/Engine.h
	source/Engine.cpp
	source/Application.h
	source/Application.cpp
	source/Input/InputManager.cpp
  	source/Input/InputManager.h
	source/Graphics/ShaderProgram.cpp
	source/Graphics/ShaderProgram.h
	source/Graphics/GraphicsAPI.h
	source/Graphics/GraphicsAPI.cpp
)

include_directories(source)
add_library(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})

target_include_directories(${PROJECT_NAME}

    PUBLIC

        source

        thirdparty/glfw-3.4/include

        thirdparty/glew/include

)

# Add GLFW library
add_subdirectory(thirdparty/glfw-3.4 "${CMAKE_CURRENT_BINARY_DIR}/glfw_build")
include_directories(thirdparty/glfw-3.4/include)

# Add Glew library
set(BUILD_UTILS OFF CACHE BOOL "utilities" FORCE)
add_subdirectory(thirdparty/glew/build/cmake "${CMAKE_CURRENT_BINARY_DIR}/glew_build")
include_directories(thirdparty/glew/include)
set_property(TARGET glew_s PROPERTY FOLDER GLEW)
set_property(TARGET glew PROPERTY FOLDER GLEW)

# Link all thirdparty libraries
target_link_libraries(${PROJECT_NAME}

    PUBLIC

        glfw

        glew_s

)
