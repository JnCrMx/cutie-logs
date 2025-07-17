cmake_minimum_required(VERSION 3.31.6)

include(FetchContent)
include(ExternalProject)

project(cutie-logs)

option(GENERATE_POT "Generate .pot file" OFF)

if(GENERATE_POT)
    FetchContent_Declare(i18n++
    GIT_REPOSITORY https://github.com/JnCrMx/i18n-cpp.git
    GIT_TAG trunk
    GIT_SUBMODULES_RECURSE FALSE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_PLUGIN ON CACHE BOOL "" FORCE)
    set(BUILD_MERGE ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(i18n++)
endif()

ExternalProject_Add(frontend
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/frontend
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    CMAKE_CACHE_ARGS
        -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=${CMAKE_EXPORT_COMPILE_COMMANDS}
        -DGENERATE_POT:BOOL=${GENERATE_POT}
        -Di18n-merge-pot_LOCATION:FILEPATH=$<TARGET_FILE:i18n::i18n-merge-pot>
        -Di18n-plugin_LOCATION:FILEPATH=$<TARGET_FILE:$<INSTALL_INTERFACE:i18n::>plugin>
)

function(AddExternalProjectFileDependency
         ExternalProjectName
         TargetName
         GeneratedFilePaths)
    include(ExternalProject)
    if(NOT TARGET ${ExternalProjectName})
        message(FATAL_ERROR "External project '${ExternalProjectName}' is not defined.")
    endif()
    ExternalProject_Get_Property(${ExternalProjectName} BINARY_DIR)
    set(GENERATED_FILES)
    foreach(FILE ${GeneratedFilePaths})
        list(APPEND GENERATED_FILES "${BINARY_DIR}/${FILE}")
    endforeach()
    add_custom_command(
        OUTPUT ${GENERATED_FILES}
        DEPENDS ${ExternalProjectName}
        COMMENT "Waiting for generated files from ExternalProject \"${ExternalProjectName}\""
        COMMAND ${CMAKE_COMMAND} -E echo "Waiting for generated files from ExternalProject \"${ExternalProjectName}\""
    )
    add_custom_target(${TargetName} DEPENDS ${GENERATED_FILES})
endfunction()


AddExternalProjectFileDependency(frontend frontend_files
    mainpage.html
    style.css
    webpp.js
    mainpage.wasm
    mainpage.opt.wasm
)

add_subdirectory(backend)
