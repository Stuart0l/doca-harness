add_executable(dma_server dma_server.cc dma_common.cc)
add_executable(dma_client dma_client.cc dma_common.cc)

target_link_libraries(dma_server doca-harness)
target_link_libraries(dma_client doca-harness)
