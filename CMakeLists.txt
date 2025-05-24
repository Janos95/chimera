cmake_minimum_required(VERSION 3.28)
project(hybrid_modeling LANGUAGES CXX)

message(STATUS "Enabling AddressSanitizer and UndefinedBehaviorSanitizer flags")
add_compile_options(-g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
add_link_options(-fsanitize=address -fsanitize=undefined)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenMP REQUIRED)

include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
FetchContent_MakeAvailable(doctest)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)

FetchContent_MakeAvailable(SFML)

# ImGui and ImGui-SFML integration
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.1
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v3.0
)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF)
set(IMGUI_SFML_IMGUI_DEMO ON)
FetchContent_MakeAvailable(imgui-sfml)

add_library(hybrid_modeling_lib
    vm.cpp
    node.cpp
    compiler.cpp
    marching_squares.cpp
)

add_executable(hybrid_modeling main.cpp)
target_link_libraries(hybrid_modeling PRIVATE SFML::Graphics SFML::Window SFML::System hybrid_modeling_lib ImGui-SFML::ImGui-SFML)

add_executable(hybrid_modeling_tests tests.cpp)
target_link_libraries(hybrid_modeling_tests PRIVATE hybrid_modeling_lib doctest::doctest)
