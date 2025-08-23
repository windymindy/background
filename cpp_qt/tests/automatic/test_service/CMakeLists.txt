cmake_minimum_required (VERSION 3.16)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (
    test_service
    LANGUAGES CXX
)

find_package (Qt6 REQUIRED COMPONENTS Test)
enable_testing (true)
set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)

find_package (background REQUIRED)

qt6_add_executable (test_service)
add_test (NAME test_service COMMAND test_service)

target_sources (
    test_service PRIVATE
    test_service.cpp
)

target_link_libraries (
    test_service PRIVATE
    Qt::Test
)
target_link_libraries (
    test_service PRIVATE
    background
)
