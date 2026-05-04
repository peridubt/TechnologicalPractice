cmake_minimum_required(VERSION 3.20)
project(TechnologicalPractice CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glfw)

set(ASSIMP_BUILD_TESTS                    OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                        OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB                     ON  CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER             ON  CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG        v5.4.3
        GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  do-not-build)
FetchContent_MakeAvailable(eigen)
add_library(eigen_headers INTERFACE)
target_include_directories(eigen_headers INTERFACE ${eigen_SOURCE_DIR})

FetchContent_Declare(libigl
        GIT_REPOSITORY https://github.com/libigl/libigl.git
        GIT_TAG        v2.5.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  do-not-build)
FetchContent_MakeAvailable(libigl)
add_library(igl_headers INTERFACE)
target_include_directories(igl_headers INTERFACE ${libigl_SOURCE_DIR}/include)
target_link_libraries(igl_headers INTERFACE eigen_headers)

add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE third_party/stb)

# Целевой исполняемый файл
find_package(OpenGL REQUIRED)

add_executable(TechnologicalPractice main.cpp
        graphics-components/shader.h
        graphics-components/camera.h
        graphics-components/mesh.h
        graphics-components/line_mesh.h
        graphics-components/igl_bridge.h
        graphics-components/contours.h
        loader/model.h
        loader/filesystem.h
)

target_include_directories(TechnologicalPractice PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(TechnologicalPractice PRIVATE
        glfw
        glad
        glm::glm
        stb_headers
        eigen_headers
        igl_headers
        assimp
        OpenGL::GL
)

if (WIN32)
    target_compile_definitions(TechnologicalPractice PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX)
    # comdlg32 -- системный диалог выбора файла (GetOpenFileNameA)
    target_link_libraries(TechnologicalPractice PRIVATE comdlg32)
endif()

# Под MinGW линкуем рантайм статически, чтобы .exe был самодостаточным
if (MINGW)
    target_link_options(TechnologicalPractice PRIVATE
            -static -static-libgcc -static-libstdc++)
endif()

# Копируем ресурсы (шейдеры, модели) рядом с .exe после сборки
add_custom_command(TARGET TechnologicalPractice POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/shaders
                $<TARGET_FILE_DIR:TechnologicalPractice>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/models
                $<TARGET_FILE_DIR:TechnologicalPractice>/models)
