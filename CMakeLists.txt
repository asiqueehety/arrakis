cmake_minimum_required(VERSION 3.16)

project(Arrakis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    Arrakis
    arrakis.cpp
    glad.c
)

target_include_directories(
    Arrakis
    PRIVATE
    "E:/glfw_nec/Include/include"
    "E:/glm"
)

target_link_directories(
    Arrakis
    PRIVATE
    "E:/glfw_nec/Libs"
)

target_compile_definitions(
    Arrakis
    PRIVATE
    GLFW_INCLUDE_NONE
)

target_link_libraries(
    Arrakis
    PRIVATE
    glfw3
    opengl32
)