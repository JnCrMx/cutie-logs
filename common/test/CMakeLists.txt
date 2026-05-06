add_executable(test_stencil "stencil.cpp")
target_link_libraries(test_stencil PRIVATE common)

add_executable(test_mmdb "mmdb.cpp")
target_link_libraries(test_mmdb PRIVATE common)

add_executable(test_parse_ip "parse_ip.cpp")
target_link_libraries(test_parse_ip PRIVATE common)
