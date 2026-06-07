cmake_minimum_required(VERSION 3.21)
project(BigFreeze CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# --- Unit tests for the pure physics model (no OpenGL) ---
add_executable(test_cosmology tests/test_cosmology.cpp)
target_include_directories(test_cosmology PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME cosmology COMMAND test_cosmology)
