add_library(common STATIC)
target_sources(common PUBLIC FILE_SET CXX_MODULES FILES
    common.cppm
    glaze.cppm
    mmdb.cppm
    stencil_functions.cppm
    stencil.cppm
    structs.cppm
    utils.cppm
)
target_compile_features(common PUBLIC cxx_std_23)
target_link_libraries(common PUBLIC glazeModule)

if(CUTIE_LOGS_BUILD_TESTS AND (${BUILD_TARGET} STREQUAL "backend"))
    add_subdirectory("test")
endif()
