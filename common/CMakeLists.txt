add_library(common STATIC)
target_sources(common PUBLIC FILE_SET CXX_MODULES FILES common.cppm structs.cppm)
target_compile_features(common PUBLIC cxx_std_23)
target_link_libraries(common PUBLIC glazeModule)
