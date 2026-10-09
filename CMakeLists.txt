cmake_minimum_required(VERSION 3.20)

project(Arrakis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    Arrakis
    arrakis.cpp
    glad.c
    arrakis_game.h
    arrakis_worm.h
    arrakis_hud.h
    arrakis_tests.h
)

find_package(OpenGL REQUIRED)
# Existing laboratory installs remain hints; every path can be overridden in CMake.
find_path(GLAD_INCLUDE_DIR glad/glad.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp HINTS "E:/glm" "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include" REQUIRED)
find_library(GLFW_LIBRARY NAMES glfw3 glfw HINTS "E:/glfw_nec/Libs" REQUIRED)
target_include_directories(Arrakis PRIVATE ${GLAD_INCLUDE_DIR} ${GLFW_INCLUDE_DIR} ${GLM_INCLUDE_DIR})

target_compile_definitions(
    Arrakis
    PRIVATE
    GLFW_INCLUDE_NONE
)

target_link_libraries(
    Arrakis
    PRIVATE
    ${GLFW_LIBRARY}
    OpenGL::GL
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME RescueMissionLogic COMMAND Arrakis --test-game)
endif()
