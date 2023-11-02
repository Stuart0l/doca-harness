add_executable(chan_client chan_client.cc ch_common.cc)
add_executable(chan_server chan_server.cc ch_common.cc)

target_link_libraries(chan_client doca-harness)
target_link_libraries(chan_server doca-harness)
