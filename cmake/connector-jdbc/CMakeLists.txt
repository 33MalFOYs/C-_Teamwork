cmake_minimum_required(VERSION 3.21)
project(ExpenseTrackerConnectorBuild LANGUAGES C CXX)

if(NOT CONNECTOR_SOURCE_ROOT)
    message(FATAL_ERROR "Set CONNECTOR_SOURCE_ROOT to the extracted Connector/C++ 8.1.0 source.")
endif()

# Configure only Oracle's classic JDBC component. Its standalone project omits
# install layout/version metadata, so supply the official version here while
# avoiding unrelated X Protocol/protobuf/OpenSSL development dependencies.
include("${CONNECTOR_SOURCE_ROOT}/version.cmake")
set(BUILD_STATIC ON CACHE BOOL "Build with the application's compiler")
set(BUNDLE_DEPENDENCIES OFF CACHE BOOL "Runtime staging belongs to the app")
set(MYSQLCLIENT_STATIC_LINKING OFF CACHE BOOL "Use the MySQL C client DLL")
set(MYSQLCLIENT_STATIC_BINDING ON CACHE BOOL "Link the C API import library")
set(INSTALL_INCLUDE_DIR include)
set(INSTALL_LIB_DIR lib)
set(INSTALL_LIB_DIR_STATIC lib)
set(INSTALL_DOC_DIR share/doc)
add_subdirectory("${CONNECTOR_SOURCE_ROOT}/jdbc" jdbc)
if(MINGW)
    target_compile_options(jdbc PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/mingw_compat.hpp")
endif()

# The jdbc archive already contains every classic connector translation unit.
# Build that target and stage it with its explicit C API dependency. Oracle's
# aggregate connector-jdbc target uses an unquoted merge helper on Windows,
# which cannot handle workspace paths containing spaces; it is unnecessary
# when the client DLL import library remains a separate link dependency.
