cmake_minimum_required(VERSION 3.31)

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
)
add_subdirectory(backend)
