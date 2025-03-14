add_library(common STATIC)
target_sources(common PUBLIC FILE_SET CXX_MODULES FILES common.cppm)
target_compile_features(common PUBLIC cxx_std_23)
