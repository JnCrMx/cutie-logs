cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

project(backend)
set(BUILD_TARGET backend)

find_package(Protobuf REQUIRED)
find_package(ZLIB REQUIRED)

FetchContent_Declare(opentelemetry-proto
  GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-proto.git
  GIT_TAG        v1.5.0)
FetchContent_Declare(pistache
  GIT_REPOSITORY https://github.com/pistacheio/pistache.git
  GIT_TAG        master)
FetchContent_Declare(gzip-hpp
  GIT_REPOSITORY https://github.com/mapbox/gzip-hpp.git
  GIT_TAG        master
  SOURCE_SUBDIR  include)
FetchContent_Declare(pqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.10.0)
FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG        v3.2)

set(PISTACHE_BUILD_TESTS OFF CACHE BOOL "Pistache: Build tests" FORCE)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(opentelemetry-proto pistache gzip-hpp pqxx argparse)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)

add_library(protoModule STATIC)
set(proto_files
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/common/v1/common.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/resource/v1/resource.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/logs/v1/logs.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/collector/logs/v1/logs_service.proto
)
protobuf_generate(
  TARGET protoModule
  LANGUAGE cpp
  PROTOS ${proto_files}
  IMPORT_DIRS ${opentelemetry-proto_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)
target_include_directories(protoModule PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(protoModule PUBLIC FILE_SET CXX_MODULES FILES
  proto.cppm
)

set(SOURCES
  main.cpp
  web/api.cpp
  web/static.cpp
)
set(MODULE_SOURCES
  backend.cppm
  opentelemetry/server.cppm
  web/server.cppm
)

add_executable(server ${SOURCES})
target_sources(server PRIVATE FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
target_link_libraries(server PRIVATE protoModule pistacheModule gzipModule pqxxModule argparseModule protobuf::libprotobuf)
target_compile_options(server PRIVATE --embed-dir=${CMAKE_BINARY_DIR}/frontend/ -Wno-c23-extensions)
add_dependencies(server frontend_mainpage)
