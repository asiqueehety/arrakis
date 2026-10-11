cmake_minimum_required(VERSION 3.20)
project(ArrakisPresentationCapture LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(OpenGL REQUIRED)
find_path(GLAD_INCLUDE_DIR glad/glad.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h HINTS "E:/glfw_nec/Include/include" REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp HINTS "E:/glm" REQUIRED)
find_library(GLFW_LIBRARY NAMES glfw3 glfw HINTS "E:/glfw_nec/Libs" REQUIRED)
add_executable(Capture capture.cpp
    "${PROJECT_ROOT}/third_party/glad/glad.c"
    "${PROJECT_ROOT}/third_party/miniaudio/miniaudio_impl.cpp")
target_include_directories(Capture PRIVATE ${GLAD_INCLUDE_DIR} ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR} "${PROJECT_ROOT}/tests" "${PROJECT_ROOT}/third_party/miniaudio")
target_compile_definitions(Capture PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(Capture PRIVATE ${GLFW_LIBRARY} OpenGL::GL)
