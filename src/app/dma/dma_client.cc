#include <doca_argp.h>
#include <doca_error.h>
#include <doca_log.h>

#include "chan/comm_channel.h"
#include "dma/dma.h"
#include "dma_common.h"

const char *server_name = "doca_dma_server";

DOCA_LOG_REGISTER(DMA_CLIENT::MAIN);

int main(int argc, char *argv[]) {
    using namespace doca;
    doca_error_t result;
    struct dma_copy_cfg dma_cfg;
    doca_app_mode mode = DOCA_MODE_HOST;

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
        DOCA_LOG_ERR("Failed to register DMA copy client parameters: %s", doca_get_error_string(result));
        doca_argp_destroy();
        return result;
    }
    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to parse application input: %s", doca_get_error_string(result));
        return result;
    }

    CommChannel ch(mode, dma_cfg.cc_dev_pci_addr, dma_cfg.cc_dev_rep_pci_addr);

    result = ch.Connect(server_name);
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }
    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }

    DOCADma dma(mode);
    MemMap mmap;

    mmap.AllocAndPopulate(DOCA_ACCESS_DPU_READ_WRITE, dma_cfg.chunk_size);
    dma.Init(mmap);

    dma.ExportDesc(mmap, ch);  // -->
    mmap.SendAddrAndOffset(ch);  // -->

    ch.WaitForSuccessfulMsg();
    DOCA_LOG_INFO("Final status message was successfully received");

argp_cleanup:
    doca_argp_destroy();

    return result;
}
