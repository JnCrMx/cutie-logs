cmake_minimum_required(VERSION 3.31.6)

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

FetchContent_Declare(webpp
  GIT_REPOSITORY https://github.com/JnCrMx/webpp.git
  GIT_TAG main)
FetchContent_Declare(webxx
  GIT_REPOSITORY https://github.com/rthrfrd/webxx.git
  GIT_TAG v0.9.2)
FetchContent_Declare(glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG v5.1.2
  GIT_SHALLOW TRUE)

add_compile_options(-fno-rtti -fno-exceptions -ftemplate-backtrace-limit=0)
FetchContent_MakeAvailable(webxx glaze webpp)

add_library(stdModule STATIC)
target_sources(stdModule PUBLIC FILE_SET CXX_MODULES BASE_DIRS /usr/lib/llvm-19/share/libc++/v1/
    FILES /usr/lib/llvm-19/share/libc++/v1/std.cppm)
target_compile_features(stdModule PUBLIC cxx_std_26)
target_compile_options(stdModule PRIVATE -Wno-reserved-identifier)
target_compile_definitions(stdModule PRIVATE _LIBCPP_CSETJMP _LIBCPP_CSIGNAL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_BINARY_DIR}/common)

set(SOURCES
  src/mainpage.cpp
  src/c++support.cpp
  src/assets.cpp
)
set(MODULE_SOURCES
  src/assets.cppm
  src/utils.cppm
  src/components/_components.cppm
  src/components/alert.cppm
  src/components/dialog_add.cppm
  src/components/dialog_delete.cppm
  src/components/duration_picker.cppm
  src/components/resource_modal.cppm
  src/components/selection.cppm
  src/components/theme_button.cppm
  src/components/utils.cppm
  src/pages/_pages.cppm
  src/pages/page.cppm
  src/pages/logs.cppm
  src/pages/table.cppm
  src/pages/settings.cppm
)

add_executable(mainpage ${SOURCES})
target_sources(mainpage PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
target_compile_features(mainpage PRIVATE cxx_std_26)
target_link_libraries(mainpage PRIVATE common stdModule webxxModule glazeModule webpp::webpp)
target_link_options(mainpage PRIVATE -Wl,--no-entry)
target_link_options(mainpage PRIVATE -nostartfiles)
target_compile_options(mainpage PRIVATE --embed-dir=${CMAKE_CURRENT_SOURCE_DIR} -Wno-c23-extensions)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mainpage.html
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/public/mainpage.html
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/public/mainpage.html ${CMAKE_CURRENT_BINARY_DIR}/mainpage.html
)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/style.css
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/public/style.css
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/public/style.css ${CMAKE_CURRENT_BINARY_DIR}/style.css
)
add_custom_target(mainpage_public ALL DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/mainpage.html ${CMAKE_CURRENT_BINARY_DIR}/style.css)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mainpage.opt.wasm
  DEPENDS $<TARGET_FILE:mainpage>
  COMMAND wasm-strip $<TARGET_FILE:mainpage> -o ${CMAKE_CURRENT_BINARY_DIR}/mainpage.opt.wasm
)
add_custom_target(mainpage_opt ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mainpage.opt.wasm)

FetchContent_GetProperties(webpp)
add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E copy ${webpp_SOURCE_DIR}/webpp.js ${CMAKE_CURRENT_BINARY_DIR}/webpp.js
    DEPENDS ${webpp_SOURCE_DIR}/webpp.js
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/webpp.js
)
add_custom_target(copy_webpp_js ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/webpp.js)
