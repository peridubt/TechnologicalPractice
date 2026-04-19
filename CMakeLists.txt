cmake_minimum_required(VERSION 4.0)
project(TechnologicalPractice)

find_package(assimp CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_executable(TechnologicalPractice main.cpp
        graphics-components/shader.h
        graphics-components/camera.h
        loader/model.h
        graphics-components/mesh.h
        loader/filesystem.h
)
target_link_libraries(TechnologicalPractice
        PRIVATE assimp::assimp
        glad::glad
        glfw
        glm::glm
        opengl32
)


include_directories(
        "${CMAKE_SOURCE_DIR}")