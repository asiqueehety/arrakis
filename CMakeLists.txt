cmake_minimum_required(VERSION 3.20)

project(Arrakis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    Arrakis
    src/arrakis.cpp
    third_party/glad/glad.c
    third_party/miniaudio/miniaudio_impl.cpp
    src/arrakis_audio.h
    src/arrakis_game.h
    src/arrakis_worm.h
    src/arrakis_hud.h
    tests/arrakis_tests.h
)

find_package(OpenGL REQUIRED)
# Existing laboratory installs remain hints; every path can be overridden in CMake.
find_path(GLAD_INCLUDE_DIR glad/glad.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp HINTS "E:/glm" "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include" REQUIRED)
find_library(GLFW_LIBRARY NAMES glfw3 glfw HINTS "E:/glfw_nec/Libs" REQUIRED)
target_include_directories(Arrakis PRIVATE
    ${GLAD_INCLUDE_DIR} ${GLFW_INCLUDE_DIR} ${GLM_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio"
)

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

# Keep runtime audio beside the executable for IDE, CTest, and standalone launches.
add_custom_command(TARGET Arrakis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio"
        "$<TARGET_FILE_DIR:Arrakis>/assets/audio"
)
set_property(TARGET Arrakis PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Arrakis>")

include(CTest)
if(BUILD_TESTING)
    add_test(NAME RescueMissionLogic COMMAND Arrakis --test-game)
endif()
