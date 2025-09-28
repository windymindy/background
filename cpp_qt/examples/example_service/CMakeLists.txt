cmake_minimum_required (VERSION 3.16)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_INSTALL_BINDIR ".")
set (CMAKE_INSTALL_LIBDIR ".")
set (CMAKE_INSTALL_LIBEXECDIR ".")

project (
    example_service
    DESCRIPTION "Example service."
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package (Qt6 REQUIRED COMPONENTS Core)
qt6_standard_project_setup ()

find_package (background REQUIRED)

qt6_add_executable (${PROJECT_NAME})

target_sources (
    ${PROJECT_NAME} PRIVATE FILE_SET headers_private TYPE HEADERS
    FILES
    logger.hpp
)
target_sources (
    ${PROJECT_NAME} PRIVATE
    main.cpp
    logger.cpp
)
source_group (
    ${PROJECT_NAME}
    FILES
    main.cpp
    logger.hpp
    logger.cpp
    application.hpp
)

target_link_libraries (
    example_service PRIVATE
    Qt6::Core
)
target_link_libraries (
    example_service PRIVATE
    background
)

install (TARGETS example_service)
qt6_generate_deploy_app_script (
    TARGET example_service
    OUTPUT_SCRIPT install_dependencies
    NO_TRANSLATIONS
)
install (SCRIPT ${install_dependencies})
