cmake_minimum_required (VERSION 3.25)

project (
    background
    DESCRIPTION "Background library."
    VERSION 1.0.0
    LANGUAGES CXX
)

include (./settings.cmake)
include (CMakePackageConfigHelpers)

option (BUILD_SHARED_LIBS "Build dynamic (on) or static (off) libraries." OFF)

find_package (Qt6 REQUIRED COMPONENTS Core)
qt6_standard_project_setup ()
qt_policy (SET QTP0003 NEW)

set (library ${PROJECT_NAME})
qt6_add_library (${library})

set (sources "../sources/background")
target_sources (
    ${library} PUBLIC FILE_SET HEADERS
    BASE_DIRS ../sources/
    FILES
    ${sources}/background_application.hpp
    ${sources}/application
    ${sources}/background_datatypes.hpp
    ${sources}/background_datatypes_forward.hpp
    ${sources}/background_event_loop_controller.hpp
    ${sources}/background_service_platform.hpp
    ${sources}/background_console_platform.hpp
    ${sources}/background_library.hpp
)
target_sources (
    ${library} PRIVATE FILE_SET headers_private TYPE HEADERS
    BASE_DIRS ../sources/
    FILES
    ${sources}/background_event_loop_controller_qt.hpp
)
target_sources (
    ${library} PRIVATE
    ${sources}/background_application.cpp
    ${sources}/background_event_loop_controller_qt.cpp
)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources (
        ${library} PRIVATE FILE_SET headers_private TYPE HEADERS
        BASE_DIRS ../sources/
        FILES
        ${sources}/background_service_platform_windows.hpp
        ${sources}/background_console_platform_windows.hpp
    )
    target_sources (
        ${library} PRIVATE
        ${sources}/background_service_platform_windows.cpp
        ${sources}/background_console_platform_windows.cpp
    )
endif ()
source_group (
    ${PROJECT_NAME}
    FILES
    ${sources}/background_application.hpp
    ${sources}/background_application.cpp
    ${sources}/application
    ${sources}/background_datatypes.hpp
    ${sources}/background_datatypes_forward.hpp
    ${sources}/background_event_loop_controller.hpp
    ${sources}/background_event_loop_controller_qt.hpp
    ${sources}/background_event_loop_controller_qt.cpp
    ${sources}/background_service_platform.hpp
    ${sources}/background_service_platform_windows.hpp
    ${sources}/background_service_platform_windows.cpp
    ${sources}/background_console_platform.hpp
    ${sources}/background_console_platform_windows.hpp
    ${sources}/background_console_platform_windows.cpp
    ${sources}/background_library.hpp
)

if (BUILD_SHARED_LIBS STREQUAL "ON")
    target_compile_definitions (${library} PRIVATE background_library_exporting)
    target_compile_definitions (${library} INTERFACE background_library_importing)
endif ()

target_link_libraries (
    ${library} PRIVATE
    Qt6::Core
)

block ()
    # https://bugreports.qt.io/browse/QTCREATORBUG-27201
    set (CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_LIBDIR})
    install (
        TARGETS ${library}
        EXPORT ${PROJECT_NAME}
        FILE_SET HEADERS
    )
    install (
        EXPORT ${PROJECT_NAME}
        FILE ${PROJECT_NAME}.import.cmake
        DESTINATION "${CMAKE_INSTALL_PACKAGE_DESTINATION}"
    )
endblock ()

file (
    WRITE "${CMAKE_CURRENT_BINARY_DIR}/package/${PROJECT_NAME}.package.input.cmake"
[[
@PACKAGE_INIT@

include (CMakeFindDependencyMacro)

find_dependency (Qt6 COMPONENTS Core)

include ("${CMAKE_CURRENT_LIST_DIR}/@PROJECT_NAME@.import.cmake")
check_required_components (@PROJECT_NAME@)
]]
)
configure_package_config_file (
    "${CMAKE_CURRENT_BINARY_DIR}/package/${PROJECT_NAME}.package.input.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/package/${PROJECT_NAME}-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_PACKAGE_DESTINATION}"
)
write_basic_package_version_file (
    package/${PROJECT_NAME}-config-version.cmake
    COMPATIBILITY SameMajorVersion
)
install (
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/package/${PROJECT_NAME}-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/package/${PROJECT_NAME}-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_PACKAGE_DESTINATION}"
)
