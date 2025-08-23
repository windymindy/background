cmake_minimum_required (VERSION 3.16)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (
    example_service
    DESCRIPTION "Example service."
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package (Qt6 REQUIRED COMPONENTS Core)
qt6_standard_project_setup ()

find_package (background REQUIRED)

qt6_add_executable (example_service)
target_sources (example_service PRIVATE main.cpp)

target_link_libraries (
    example_service PRIVATE
    Qt6::Core
)
target_link_libraries (
    example_service PRIVATE
    background
)

# todo create distribution with qt deploy
