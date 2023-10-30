add_library(doca-harness SHARED)

include_directories(/opt/mellanox/doca/include)
set(DOCA_LIB_PATH /opt/mellanox/doca/lib/x86_64-linux-gnu)

target_link_libraries(doca-harness
    PRIVATE ${DOCA_LIB_PATH}/libdoca_common.so
    PRIVATE ${DOCA_LIB_PATH}/libdoca_argp.so
    PRIVATE ${DOCA_LIB_PATH}/libdoca_comm_channel.so
)

add_subdirectory(chan)
add_subdirectory(dev)
add_subdirectory(mem)
add_subdirectory(dma)
