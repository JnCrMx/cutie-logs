cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

project(frontend)
set(BUILD_TARGET frontend)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)
set(TARGET_TRIPLE wasm32-wasi)

set(CMAKE_C_COMPILER_TARGET   ${TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})

set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_LINK_DEPENDS_USE_LINKER OFF)

FetchContent_Declare(webxx
  GIT_REPOSITORY https://github.com/rthrfrd/webxx.git
  GIT_TAG v0.9.2
)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

add_compile_options(-fno-rtti -fno-exceptions)
FetchContent_MakeAvailable(webxx json)

add_library(stdModule STATIC)
target_sources(stdModule PUBLIC FILE_SET CXX_MODULES BASE_DIRS /usr/lib/llvm-19/share/libc++/v1/
    FILES /usr/lib/llvm-19/share/libc++/v1/std.cppm)
target_compile_features(stdModule PUBLIC cxx_std_26)
target_compile_options(stdModule PRIVATE -Wno-reserved-identifier)
target_compile_definitions(stdModule PRIVATE _LIBCPP_CSETJMP _LIBCPP_CSIGNAL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)

set(SOURCES
  mainpage.cpp
)
set(MODULE_SOURCES
  web.cppm
)

add_executable(mainpage ${SOURCES})
target_sources(mainpage PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
target_compile_features(mainpage PRIVATE cxx_std_26)
target_link_libraries(mainpage PRIVATE stdModule webxxModule jsonModule)
target_link_options(mainpage PRIVATE -Wl,--no-entry)
target_link_options(mainpage PRIVATE -nostartfiles)
