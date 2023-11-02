add_library(doca-harness SHARED)

include_directories(/opt/mellanox/doca/include)

if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL x86_64)
    message("Arch: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    set(DOCA_LIB_PATH /opt/mellanox/doca/lib/x86_64-linux-gnu)
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
    message("Arch: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    set(DOCA_LIB_PATH /opt/mellanox/doca/lib/aarch64-linux-gnu/)
else()
    message("Unsupported arch: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

target_link_libraries(doca-harness
    PRIVATE ${DOCA_LIB_PATH}/libdoca_common.so
    PRIVATE ${DOCA_LIB_PATH}/libdoca_argp.so
    PRIVATE ${DOCA_LIB_PATH}/libdoca_comm_channel.so
)

add_subdirectory(chan)
add_subdirectory(dev)
add_subdirectory(mem)
add_subdirectory(dma)
