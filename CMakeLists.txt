cmake_minimum_required(VERSION 3.30)

include(ExternalProject)

project(cutie-logs)

ExternalProject_Add(frontend
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/frontend
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    CMAKE_CACHE_ARGS
        -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=${CMAKE_EXPORT_COMPILE_COMMANDS}
)
add_library(frontend_mainpage STATIC IMPORTED)
set_property(
    TARGET frontend_mainpage
    PROPERTY
        IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/frontend/mainpage.wasm"
        INTERFACE_DEPENDENCIES "${CMAKE_CURRENT_BINARY_DIR}/frontend/mainpage.wasm"
)

add_subdirectory(backend)
