cmake_minimum_required (VERSION 3.16)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (
    test_application
    LANGUAGES CXX
)

find_package (Qt6 REQUIRED COMPONENTS Test)
enable_testing (true)
set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)

find_package (background REQUIRED)

qt6_add_executable (test_application)
add_test (NAME test_application COMMAND test_application)

target_sources (
    test_application PRIVATE
    test_application.cpp
)

target_link_libraries (
    test_application PRIVATE
    Qt::Test
)
target_link_libraries (
    test_application PRIVATE
    background
)
