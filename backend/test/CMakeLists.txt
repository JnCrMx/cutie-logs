add_executable(test_notifications "notifications.cpp"
  ../notifications/providers/webhook.cpp
)
target_sources(test_notifications PRIVATE FILE_SET CXX_MODULES BASE_DIRS ../ FILES
  ../notifications/notifications.cppm
  ../notifications/provider.cppm
  ../utils.cppm
  ../network_ip_filter.cppm
)
target_link_libraries(test_notifications PRIVATE
  common
  pistacheModule spdlogModule glazeModule cprModule
)
