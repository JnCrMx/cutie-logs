cmake_minimum_required(VERSION 3.31.6)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

project("${TOP_LEVEL_PROJECT_NAME}-frontend" VERSION "${TOP_LEVEL_PROJECT_VERSION}")
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
  GIT_REPOSITORY https://github.com/JnCrMx/webxx.git
  GIT_TAG main)
FetchContent_Declare(glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG        v6.0.1
  GIT_SHALLOW TRUE)
FetchContent_Declare(i18n++
  GIT_REPOSITORY https://github.com/JnCrMx/i18n-cpp.git
  GIT_TAG trunk
  GIT_SUBMODULES_RECURSE FALSE)

add_compile_options(-fno-rtti -fno-exceptions -ftemplate-backtrace-limit=0)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_PLUGIN OFF CACHE BOOL "" FORCE)
set(BUILD_MERGE OFF CACHE BOOL "" FORCE)
set(NO_LIBINTL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(webxx glaze webpp i18n++)

add_library(stdModule STATIC)
target_sources(stdModule PUBLIC FILE_SET CXX_MODULES BASE_DIRS /usr/lib/llvm-20/share/libc++/v1/
    FILES /usr/lib/llvm-20/share/libc++/v1/std.cppm)
target_compile_features(stdModule PUBLIC cxx_std_23)
target_compile_options(stdModule PRIVATE -Wno-reserved-identifier)
target_compile_definitions(stdModule PRIVATE _LIBCPP_CSETJMP _LIBCPP_CSIGNAL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_BINARY_DIR}/common)

# read po/LINGUAS file
find_package(Gettext REQUIRED)
file(STRINGS po/LINGUAS LINGUAS)
foreach(LING ${LINGUAS})
  gettext_process_po_files(${LING} ALL PO_FILES po/${LING}.po)
endforeach()

set(SOURCES
  src/mainpage.cpp
  src/cpp_support.cpp
  src/assets.cpp
  src/translations.cpp
)
set(MODULE_SOURCES
  src/assets.cppm
  src/translations.cppm
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
target_compile_features(mainpage PRIVATE cxx_std_23)
target_link_libraries(mainpage PRIVATE common stdModule webxxModule glazeModule i18nModule webpp::webpp)
target_link_options(mainpage PRIVATE -Wl,--no-entry)
target_link_options(mainpage PRIVATE -nostartfiles)
target_compile_options(mainpage PRIVATE --embed-dir=${CMAKE_CURRENT_SOURCE_DIR} -Wno-c23-extensions)
if(GENERATE_POT)
  include(${i18n++_SOURCE_DIR}/i18n++Use.cmake)

  add_executable(i18n::i18n-merge-pot IMPORTED)
  set_target_properties(i18n::i18n-merge-pot PROPERTIES IMPORTED_LOCATION ${i18n-merge-pot_LOCATION})
  target_compile_options(i18n-lib INTERFACE "$<IF:$<CXX_COMPILER_ID:Clang>,-fplugin=${i18n-plugin_LOCATION},>")

  target_use_i18n(mainpage NODOMAIN)
endif()

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
