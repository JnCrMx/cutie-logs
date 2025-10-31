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
target_compile_features(common PUBLIC cxx_std_26)
target_link_libraries(common PUBLIC glazeModule)

if(${BUILD_TARGET} STREQUAL "backend")
    add_executable(test_stencil "test/stencil.cpp")
    target_link_libraries(test_stencil PRIVATE common)

    add_executable(test_mmdb "test/mmdb.cpp")
    target_link_libraries(test_mmdb PRIVATE common)
endif()
