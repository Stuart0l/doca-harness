#pragma once

#include <doca_mmap.h>

#include <memory>

#include "../chan/comm_channel.h"
#include "../dev/device.h"

namespace doca {

class DOCADma;

struct ExportDesc {
    const void *desc;
    size_t len;
};

class MemMap {
    friend class DOCADma;
    friend class DOCADevice;

   public:
    MemMap();
    ~MemMap();
    doca_error_t AllocAndPopulate(uint32_t access_flags, size_t buffer_len);
    doca_error_t ExportDPU(DOCADevice &dev);
    doca_error_t SendDesc(CommChannel &ch);

   protected:
    char *buffer;
    size_t len;
    struct doca_mmap *mmap;
    ExportDesc export_desc;
};

}  // namespace doca
