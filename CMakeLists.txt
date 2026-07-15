cmake_minimum_required(VERSION 3.10)
project(FileMove VERSION 1.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate .rc from template with correct asset paths
set(FILEMOVE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")

# Asset files that the .rc depends on
set(FILEMOVE_ASSET_FILES
    assets/icons/FileMove-icon.ico
    assets/images/about-image.png
    assets/images/green-check.png
    assets/images/red-x.png
    assets/images/orange-question.png
)

# Regenerate .rc whenever any asset file or the template changes
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/FileMove.rc
    COMMAND ${CMAKE_COMMAND}
        -DFILEMOVE_ASSETS_DIR="${FILEMOVE_ASSETS_DIR}"
        -DFILEMOVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DFILEMOVE_BINARY_DIR="${CMAKE_BINARY_DIR}"
        -P "${CMAKE_SOURCE_DIR}/resources/GenerateRc.cmake"
    DEPENDS
        resources/FileMove.rc.in
        resources/GenerateRc.cmake
        ${FILEMOVE_ASSET_FILES}
    MAIN_DEPENDENCY resources/FileMove.rc.in
    COMMENT "Generating FileMove.rc from assets"
)

# Custom target for standalone .rc regeneration (build.ps1 -RC)
add_custom_target(generate_rc DEPENDS ${CMAKE_BINARY_DIR}/FileMove.rc)

# nlohmann/json (header-only, single header in third-party/)

# Generate build config header with current date/time every build
set(GENERATED_BUILD_CONFIG "${CMAKE_BINARY_DIR}/GeneratedBuildConfig.h")
add_custom_target(generate_build_config
    COMMAND powershell -Command "$dt = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'; '#pragma once' + \"`n\" + '#define FILEMOVE_BUILD_DATE_STR L\"' + $dt + '\"' | Out-File -Encoding ASCII '${GENERATED_BUILD_CONFIG}'"
    COMMENT "Generating build config header"
    VERBATIM
)

# Source files
set(SOURCES
    src/main.cpp
    src/utils/cmdline_parser.cpp
    src/utils/logging.cpp
    src/data/file_io.cpp
    src/data/json_parser.cpp
    src/queue/queue_manager.cpp
    src/queue/worker_thread.cpp
    src/window/main_window.cpp
    src/window/group_list.cpp
    src/window/dialogs/about.cpp
    src/window/dialogs/status.cpp
    src/window/dialogs/settings.cpp
    src/window/dialogs/group_editor.cpp
    src/window/dialogs/queue_window.cpp
    src/window/dialogs/search.cpp
    src/window/dialogs/new_file.cpp
    src/window/dialogs/conflict_dialog.cpp
    src/drag_drop/drop_target.cpp
    src/resources/resource_loader.cpp
    ${CMAKE_BINARY_DIR}/FileMove.rc
)

# Executable
add_executable(FileMove WIN32 ${SOURCES})
add_dependencies(FileMove generate_build_config)

# Force .rc regeneration before every build to catch asset file changes
add_custom_command(
    TARGET FileMove PRE_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DFILEMOVE_ASSETS_DIR="${FILEMOVE_ASSETS_DIR}"
        -DFILEMOVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DFILEMOVE_BINARY_DIR="${CMAKE_BINARY_DIR}"
        -P "${CMAKE_SOURCE_DIR}/resources/GenerateRc.cmake"
    COMMENT "Regenerating FileMove.rc from assets"
)

# Include directories
target_include_directories(FileMove PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third-party
    ${CMAKE_SOURCE_DIR}/resources
    ${CMAKE_BINARY_DIR}
)

# Link GDI+ and Windows libraries
target_link_libraries(FileMove PRIVATE
    gdiplus
    uuid
    ole32
    oleaut32
    shell32
    shlwapi
    comctl32
    dwmapi
)

# Compile definitions
target_compile_definitions(FileMove PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    FILEMOVE_VERSION="${PROJECT_VERSION}"
)

# ==================== Test Harness ====================
enable_testing()

add_executable(test_harness
    tests/test_harness.cpp
    src/utils/cmdline_parser.cpp
    src/utils/logging.cpp
    src/data/file_io.cpp
    src/data/json_parser.cpp
    src/queue/queue_manager.cpp
)

target_include_directories(test_harness PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third-party
)

target_link_libraries(test_harness PRIVATE
    uuid
    shlwapi
    dwmapi
)

target_compile_definitions(test_harness PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_test(NAME FileMoveTests COMMAND test_harness)
