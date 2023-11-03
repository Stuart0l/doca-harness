#pragma once

#include <doca_mmap.h>
#include <doca_buf.h>

#include <memory>

#include "../chan/comm_channel.h"
#include "../dev/device.h"

namespace doca {

class DOCADma;

struct ExportDesc {
    const void *desc;
    size_t len;
};

enum mmap_mode { MMAP_MODE_LOCAL, MMAP_MODE_REMOTE };

class MemMap {
    friend class DOCADma;
    friend class DOCADevice;

   public:
    MemMap();
    MemMap(DOCADma &dma, CommChannel &ch);
    ~MemMap();
    doca_error_t AllocAndPopulate(uint32_t access_flags, size_t buffer_len);
    doca_error_t ExportDPU(DOCADevice &dev);
    doca_error_t SendDesc(CommChannel &ch);
    doca_error_t RecvDesc(CommChannel &ch);

    doca_error_t SendAddrAndOffset(CommChannel &ch);
    doca_error_t RecvAddrAndOffset(CommChannel &ch);

   protected:
    char *buffer;
    size_t len;
    struct doca_mmap *mmap;
    struct doca_buf *doca_buf;
    ExportDesc export_desc;

    mmap_mode mode;
};

}  // namespace doca
