#pragma once

#include <doca_dev.h>

namespace doca {

using jobs_check = doca_error_t (*)(struct doca_devinfo *);

class CommChannel;
class DOCADeviceRep;
class MemMap;
class DOCADma;

class DOCADevice {
    friend class DOCADeviceRep;
    friend class CommChannel;
    friend class MemMap;
    friend class DOCADma;

   public:
    DOCADevice() = default;
    doca_error_t OpenWithPci(const char *pci_addr);
    doca_error_t OpenWithCap(jobs_check func);
    doca_error_t AddMMap(MemMap &mmap);
    ~DOCADevice();

   protected:
    struct doca_dev *dev;
};

class DOCADeviceRep {
    friend class CommChannel;

   public:
    DOCADeviceRep() = default;
    doca_error_t OpenWithPci(DOCADevice &dev, enum doca_dev_rep_filter filter, const char *pci_addr);
    ~DOCADeviceRep();

   protected:
    struct doca_dev_rep *dev_rep;
};

}  // namespace doca
