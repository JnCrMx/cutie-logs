# Backend modules
if(${BUILD_TARGET} STREQUAL "backend")
    add_library(pistacheModule STATIC)
    target_sources(pistacheModule PUBLIC FILE_SET CXX_MODULES FILES pistache.cppm)
    target_compile_features(pistacheModule PUBLIC cxx_std_26)
    target_link_libraries(pistacheModule PUBLIC pistache)

    add_library(gzipModule STATIC)
    target_sources(gzipModule PUBLIC FILE_SET CXX_MODULES FILES gzip.cppm)
    target_compile_features(gzipModule PUBLIC cxx_std_26)
    target_include_directories(gzipModule PUBLIC ${gzip-hpp_SOURCE_DIR}/include)
    target_link_libraries(gzipModule PUBLIC ZLIB::ZLIB)

    add_library(pqxxModule STATIC)
    target_sources(pqxxModule PUBLIC FILE_SET CXX_MODULES FILES pqxx.cppm)
    target_compile_features(pqxxModule PUBLIC cxx_std_26)
    target_link_libraries(pqxxModule PUBLIC libpqxx::pqxx)

    add_library(argparseModule STATIC)
    target_sources(argparseModule PUBLIC FILE_SET CXX_MODULES FILES argparse.cppm)
    target_compile_features(argparseModule PUBLIC cxx_std_26)
    target_link_libraries(argparseModule PUBLIC argparse::argparse)

    add_library(spdlogModule STATIC)
    target_sources(spdlogModule PUBLIC FILE_SET CXX_MODULES FILES spdlog.cppm)
    target_compile_features(spdlogModule PUBLIC cxx_std_26)
    target_link_libraries(spdlogModule PUBLIC spdlog::spdlog)

    add_library(cprModule STATIC)
    target_sources(cprModule PUBLIC FILE_SET CXX_MODULES FILES cpr.cppm)
    target_compile_features(cprModule PUBLIC cxx_std_26)
    target_link_libraries(cprModule PUBLIC cpr::cpr)
endif()

# Frontend modules
if(${BUILD_TARGET} STREQUAL "frontend")
    add_library(webxxModule STATIC)
    target_sources(webxxModule PUBLIC FILE_SET CXX_MODULES FILES webxx.cppm)
    target_compile_features(webxxModule PUBLIC cxx_std_26)
    target_link_libraries(webxxModule PUBLIC webxx::webxx)

    add_library(i18nModule STATIC)
    target_sources(i18nModule PUBLIC FILE_SET CXX_MODULES FILES i18n.cppm)
    target_compile_features(i18nModule PUBLIC cxx_std_26)
    target_link_libraries(i18nModule PUBLIC i18n::i18n-lib)
endif()

# Common modules
add_library(glazeModule STATIC)
target_sources(glazeModule PUBLIC FILE_SET CXX_MODULES FILES glaze.cppm)
target_compile_features(glazeModule PUBLIC cxx_std_26)
target_link_libraries(glazeModule PUBLIC glaze::glaze)
target_compile_definitions(glazeModule PUBLIC BUILD_TARGET_${BUILD_TARGET})
