cmake_minimum_required(VERSION 3.21)
project(BigFreeze CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# --- Unit tests for the pure physics model (no OpenGL) ---
add_executable(test_cosmology tests/test_cosmology.cpp)
target_include_directories(test_cosmology PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME cosmology COMMAND test_cosmology)

# --- Application (OpenGL) ---
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(BigFreeze big_freeze.cpp)
target_include_directories(BigFreeze PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BigFreeze PRIVATE glfw GLEW::GLEW glm::glm OpenGL::GL)

# Copy shaders next to the executable
file(GLOB SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
                  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
add_custom_command(TARGET BigFreeze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BigFreeze>/shaders)
foreach(shader ${SHADERS})
    add_custom_command(TARGET BigFreeze POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${shader}
        $<TARGET_FILE_DIR:BigFreeze>/shaders)
endforeach()
