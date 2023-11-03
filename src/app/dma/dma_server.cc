#include <doca_argp.h>
#include <doca_error.h>
#include <doca_log.h>

#include <chrono>

#include "chan/comm_channel.h"
#include "dma/dma.h"
#include "dma_common.h"

const char *server_name = "doca_dma_server";
const int iteration = 100000;

DOCA_LOG_REGISTER(DMA_SERVER::MAIN);

int main(int argc, char *argv[]) {
    using namespace doca;
    using namespace std::chrono;

    doca_error_t result;
    struct dma_copy_cfg dma_cfg;
    doca_app_mode mode = DOCA_MODE_DPU;
    int64_t duration;

    /* Register a logger backend */
    result = doca_log_create_standard_backend();
    if (result != DOCA_SUCCESS) return result;

    result = doca_argp_init("doca_dma_copy", &dma_cfg);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
        return result;
    }
    result = register_dma_copy_params();
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register DMA copy server parameters: %s", doca_get_error_string(result));
        doca_argp_destroy();
        return result;
    }
    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to parse application input: %s", doca_get_error_string(result));
        return result;
    }

    CommChannel ch(mode, dma_cfg.cc_dev_pci_addr, dma_cfg.cc_dev_rep_pci_addr);

    result = ch.Listen(server_name);
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }
    result = ch.WaitForSuccessfulMsg();
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }

    DOCADma dma(mode);
    MemMap local_mmap;
    
    local_mmap.AllocAndPopulate(DOCA_ACCESS_LOCAL_READ_WRITE, dma_cfg.chunk_size);
    dma.Init(local_mmap);

    MemMap remote_mmap(dma, ch); // <--
    remote_mmap.RecvAddrAndOffset(ch); // <--

    dma.AddBuffer(local_mmap, remote_mmap);
    
    auto start = high_resolution_clock::now();
    decltype(start) end;

    for (int i = 0; i < iteration; i++) {
        result = dma.DmaCopy(local_mmap, remote_mmap, dma_cfg.chunk_size);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to do copy: %s", doca_get_error_string(result));
        }
    }

    end = high_resolution_clock::now();

    ch.SendSuccessfulMsg();

    duration = duration_cast<microseconds>(end - start).count();
    DOCA_LOG_INFO("Throughput: %f MB/s", static_cast<double>(dma_cfg.chunk_size * iteration) / duration);

    dma.RmBuffer(local_mmap, remote_mmap);
    dma.Finalize();
    
    return result;
}
