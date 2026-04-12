cmake_minimum_required(VERSION 3.31.6)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

project("${TOP_LEVEL_PROJECT_NAME}-backend" VERSION "${TOP_LEVEL_PROJECT_VERSION}")
set(BUILD_TARGET backend)

find_package(ZLIB REQUIRED)

FetchContent_Declare(opentelemetry-proto
  GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-proto.git
  GIT_TAG        v1.10.0
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(pistache
  GIT_REPOSITORY https://github.com/JnCrMx/pistache.git
  GIT_TAG        cmake-optional-rapidjson
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(gzip-hpp
  GIT_REPOSITORY https://github.com/mapbox/gzip-hpp.git
  GIT_TAG        master
  SOURCE_SUBDIR  include
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(pqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.10.5
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG        v3.2
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG        v7.0.2
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        1.14.2
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_Declare(hpp_proto
  GIT_REPOSITORY https://github.com/huangminghuang/hpp-proto.git
  GIT_TAG main
  GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)

set(PISTACHE_BUILD_TESTS OFF CACHE BOOL "Pistache: Build tests" FORCE)
set(PISTACHE_USE_RAPIDJSON OFF CACHE BOOL "Pistache: Use RapidJSON" FORCE)
set(CPR_ENABLE_CURL_HTTP_ONLY OFF CACHE BOOL "CPR: Enable only HTTP" FORCE)
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "CPR: Use system libcurl" FORCE)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HPP_PROTO_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(opentelemetry-proto pistache gzip-hpp pqxx argparse glaze cpr hpp_proto)

find_package(spdlog REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_BINARY_DIR}/common)

set(proto_files
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/common/v1/common.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/resource/v1/resource.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/logs/v1/logs.proto
  ${opentelemetry-proto_SOURCE_DIR}/opentelemetry/proto/collector/logs/v1/logs_service.proto
)
add_library(protoGen INTERFACE ${proto_files})
protobuf_generate_hpp(TARGET protoGen
  IMPORT_DIRS ${opentelemetry-proto_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(protoModule STATIC)
target_sources(protoModule PUBLIC FILE_SET CXX_MODULES FILES
  proto.cppm
)
target_link_libraries(protoModule PUBLIC protoGen)

set(SOURCES
  main.cpp
  database/consistency.cpp
  database/migrations.cpp
  jobs/cleanup.cpp
  notifications/providers/webhook.cpp
  web/api.cpp
  web/static.cpp
)
set(MODULE_SOURCES
  backend.cppm
  self_sink.cppm
  utils.cppm
  network_ip_filter.cppm
  database/database.cppm
  jobs/jobs.cppm
  notifications/notifications.cppm
  notifications/provider.cppm
  opentelemetry/server.cppm
  web/server.cppm
)

add_executable(server ${SOURCES})
target_sources(server PRIVATE FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
target_link_libraries(server PRIVATE
  common protoModule
  pistacheModule gzipModule pqxxModule argparseModule spdlogModule glazeModule cprModule)
target_compile_options(server PRIVATE --embed-dir=${CMAKE_BINARY_DIR}/frontend/ --embed-dir=${CMAKE_SOURCE_DIR} -Wno-c23-extensions)
add_dependencies(server frontend_files)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/migrations.inc
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/database/generate_migrations.sh ${CMAKE_CURRENT_SOURCE_DIR}/database/migrations ${CMAKE_CURRENT_BINARY_DIR}/migrations.inc
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/database/generate_migrations.sh ${CMAKE_CURRENT_SOURCE_DIR}/database/migrations
)
target_sources(server PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/migrations.inc)

if(CUTIE_LOGS_BUILD_TESTS)
  add_subdirectory("test")
endif()
