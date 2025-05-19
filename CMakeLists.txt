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

add_executable(hybrid_modeling 
    main.cpp
    vm.cpp
    node.cpp
    compiler.cpp
)
target_link_libraries(hybrid_modeling PRIVATE SFML::Graphics SFML::Window SFML::System OpenMP::OpenMP_CXX)

add_executable(hybrid_modeling_tests
    tests.cpp
    vm.cpp
    node.cpp
    compiler.cpp
)
target_link_libraries(hybrid_modeling_tests PRIVATE doctest::doctest OpenMP::OpenMP_CXX)
