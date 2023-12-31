#pragma once

#include <doca_buf_inventory.h>
#include <doca_dma.h>

#include <memory>

#include "../chan/comm_channel.h"
#include "../common.h"
#include "../mem/mem.h"

#define WORKQ_DEPTH 32 /* Work queue depth */

namespace doca {

class DOCADma {
    friend class MemMap;
   public:
    DOCADma(doca_app_mode mode);
    ~DOCADma();

    doca_error_t Init(MemMap &mmap);
    void Finalize();
    doca_error_t ExportDesc(MemMap &mmap, CommChannel &ch);
    doca_error_t AddBuffer(MemMap &mmap);
    void RmBuffer(MemMap &mmap);
    doca_error_t DmaCopy(MemMap &from, MemMap &to, size_t size);

   protected:
    struct doca_dma *dma_ctx;
    struct doca_ctx *ctx;
    struct doca_workq *workq;
    struct doca_buf_inventory *buf_inv;

    std::shared_ptr<DOCADevice> dev;

    doca_app_mode mode;
};

}  // namespace doca
