add_library(common STATIC)
target_sources(common PUBLIC FILE_SET CXX_MODULES FILES
    common.cppm
    stencil.cppm
    stencil_functions.cppm
    structs.cppm
    mmdb.cppm
)
target_compile_features(common PUBLIC cxx_std_23)
target_link_libraries(common PUBLIC glazeModule)

if(${BUILD_TARGET} STREQUAL "backend")
    add_executable(test_stencil "test/stencil.cpp")
    target_link_libraries(test_stencil PRIVATE common)

    add_executable(test_mmdb "test/mmdb.cpp")
    target_link_libraries(test_mmdb PRIVATE common)
endif()
