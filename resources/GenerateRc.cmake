# GenerateRc.cmake - Substitutes asset paths into FileMove.rc.in
# Invoked by CMake custom command with:
#   -DFILEMOVE_ASSETS_DIR=<path> -DFILEMOVE_SOURCE_DIR=<path> -P GenerateRc.cmake
#   -DFILEMOVE_BINARY_DIR=<path>

file(READ "${FILEMOVE_SOURCE_DIR}/resources/FileMove.rc.in" RC_CONTENT)
string(REPLACE "@FILEMOVE_ASSETS_DIR@" "${FILEMOVE_ASSETS_DIR}" RC_CONTENT "${RC_CONTENT}")
file(WRITE "${FILEMOVE_BINARY_DIR}/FileMove.rc" "${RC_CONTENT}")
